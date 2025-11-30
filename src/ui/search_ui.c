/**
 * @file search_ui.c
 * @brief Search interface for tracks and artists.
 *
 * Provides UI and logic for querying the music library, filtering results,
 * and adding songs to playlists from search results.
 */

#include "search_ui.h"

#include "common/appstate.h"
#include "common/common.h"
#include "common_ui.h"

#include "data/directorytree.h"
#include "data/playlist.h"

#include "ui/queue_ui.h"

#include "utils/term.h"
#include "utils/utils.h"

#include <math.h>
#include <stdbool.h>

#define MAX_SEARCH_LEN 32

typedef struct SearchResult {
        FileSystemEntry *entry;
        struct FileSystemEntry *parent;
        int distance;
        int groupDistance;
} SearchResult;

// Global variables to store results
static SearchResult *results = NULL;
size_t results_count = 0;
size_t results_capacity = 0;
size_t terminal_height = 0;
static int num_search_letters = 0;
static int num_search_bytes = 0;
static int min_search_letters = 1;
static FileSystemEntry *current_search_entry = NULL;

static char search_text[MAX_SEARCH_LEN * 4 + 1]; // Unicode can be 4 characters

FileSystemEntry *get_current_search_entry(void)
{
        return current_search_entry;
}

int get_search_results_count(void)
{
        return results_count;
}

#define GROW_MARGIN 50

void realloc_results()
{
        if (results_count >= results_capacity) {
                results_capacity = results_capacity == 0
                                       ? 10 + GROW_MARGIN
                                       : results_capacity + GROW_MARGIN;

                results =
                    realloc(results, results_capacity * sizeof(SearchResult));
        }
}

void set_result_fields(FileSystemEntry *entry, int distance,
                       FileSystemEntry *parent)
{
        results[results_count].distance = distance;
        results[results_count].entry = entry;
        results[results_count].parent = parent;
}

bool is_duplicate(const FileSystemEntry *entry)
{
        for (size_t i = 0; i < results_count; i++) {
                const FileSystemEntry *other = results[i].entry;

                if (!entry->is_directory)
                        return false;

                if (entry == other)
                        return true;
        }

        return false;
}

void add_result(FileSystemEntry *entry, int distance)
{
        if (num_search_letters < min_search_letters)
                return;

        if (results_count > terminal_height * 10)
                return;

        if (entry->parent == NULL) // Root
                return;

        if (is_duplicate(entry))
                return;

        realloc_results();
        set_result_fields(entry, distance, NULL);
        results_count++;

        if (entry->is_directory) {
                if (entry->children && entry->parent != NULL &&
                    entry->parent->parent == NULL) {
                        FileSystemEntry *child = entry->children;

                        while (child) {
                                if (child->is_directory) {
                                        if (results_count > terminal_height * 10)
                                                break;

                                        realloc_results();
                                        set_result_fields(child, distance, entry);
                                        results_count++;
                                }

                                child = child->next;
                        }
                }
        }
}

void collect_result(FileSystemEntry *entry, int distance)
{
        add_result(entry, distance);
}

void free_search_results(void)
{
        if (results != NULL) {
                free(results);
                results = NULL;
        }

        if (current_search_entry != NULL)
                current_search_entry = NULL;

        results_capacity = 0;
        results_count = 0;
}

void calculate_group_distances(void)
{
        for (size_t i = 0; i < results_count; i++) {
                // Find top-level parent (entry with no parent, or root)
                FileSystemEntry *root = results[i].entry;
                while (root->parent != NULL) {
                        root = root->parent;
                }

                // Find if this root appears in results, and use ITS distance
                // Otherwise use minimum distance among descendants
                int minDist = results[i].distance;

                for (size_t j = 0; j < results_count; j++) {
                        FileSystemEntry *otherRoot = results[j].entry;
                        while (otherRoot->parent != NULL) {
                                otherRoot = otherRoot->parent;
                        }

                        if (otherRoot == root) {
                                if (results[j].entry == root) {
                                        // The root itself is in results - use
                                        // its distance
                                        minDist = results[j].distance;
                                        break;
                                }
                                if (results[j].distance < minDist) {
                                        minDist = results[j].distance + 1;
                                }
                        }
                }

                // If root is in results, use only its distance for grouping
                // Otherwise use the best child distance
                results[i].groupDistance = minDist;
        }
}

static int ancestor_compare(const FileSystemEntry *A, const FileSystemEntry *B)
{
        for (const FileSystemEntry *p = B->parent; p; p = p->parent)
                if (p == A)
                        return -1;
        for (const FileSystemEntry *p = A->parent; p; p = p->parent)
                if (p == B)
                        return 1;
        return 0;
}

int compare_results(const void *a, const void *b)
{
        const SearchResult *A = a;
        const SearchResult *B = b;

        int rel = ancestor_compare(A->entry, B->entry);
        if (rel != 0)
                return rel;

        // Sort by best distance in the top-level group first
        if (A->groupDistance != B->groupDistance)
                return (A->groupDistance < B->groupDistance) ? -1 : 1;

        // If different parents, compare by hierarchy (path)
        if (A->entry->parent != B->entry->parent) {
                const FileSystemEntry *p_a = A->entry;
                const FileSystemEntry *p_b = B->entry;

                // Walk up to same depth
                int depth_a = 0, depthB = 0;
                for (const FileSystemEntry *p = p_a; p->parent; p = p->parent)
                        depth_a++;
                for (const FileSystemEntry *p = p_b; p->parent; p = p->parent)
                        depthB++;

                while (depth_a > depthB) {
                        p_a = p_a->parent;
                        depth_a--;
                }
                while (depthB > depth_a) {
                        p_b = p_b->parent;
                        depthB--;
                }

                // Walk up together to find where they diverge
                while (p_a->parent != p_b->parent) {
                        p_a = p_a->parent;
                        p_b = p_b->parent;
                }

                // Compare by name at divergence point
                int cmp = strcmp(p_a->name, p_b->name);
                if (cmp != 0)
                        return cmp;
        }

        // Within same parent: directories first
        if (A->entry->is_directory != B->entry->is_directory)
                return A->entry->is_directory ? -1 : 1;

        // Then by individual distance
        if (A->distance != B->distance)
                return (A->distance < B->distance) ? -1 : 1;

        // Then by name
        int cmp = strcmp(A->entry->name, B->entry->name);
        if (cmp != 0)
                return cmp;

        return (A->entry < B->entry) ? -1 : (A->entry > B->entry);
}

void sort_search_results(void)
{
        calculate_group_distances();
        qsort(results, results_count, sizeof(SearchResult), compare_results);
}

void fuzzy_search(FileSystemEntry *root, int threshold)
{
        int term_w, term_h;
        get_term_size(&term_w, &term_h);

        terminal_height = term_h;

        free_search_results();

        if (num_search_letters > min_search_letters) {
                fuzzy_search_recursive(root, search_text, threshold,
                                       collect_result);
        }

        sort_search_results();

        trigger_refresh();
}

int display_search_box(int row, int col)
{
        AppState *state = get_app_state();
        UISettings *ui = &(state->uiSettings);

        apply_color(ui->colorMode, ui->theme.search_label, ui->color);

        printf("\033[%d;%dH", row, col);

        printf(_("   Search: "));
        apply_color(ui->colorMode, ui->theme.search_query, ui->defaultColorRGB);
        // Save cursor position
        printf("%s", search_text);
        printf("\033[s");
        printf("█");
        clear_rest_of_line();

        return 0;
}

int add_to_search_text(const char *str)
{
        if (str == NULL) {
                return -1;
        }

        size_t len = strnlen(str, MAX_SEARCH_LEN);

        // Check if the string can fit into the search text buffer
        if (num_search_letters + len > MAX_SEARCH_LEN) {
                return 0; // Not enough space
        }

        // Add the string to the search text buffer
        for (size_t i = 0; i < len; i++) {
                search_text[num_search_bytes++] = str[i];
        }

        search_text[num_search_bytes] = '\0'; // Null-terminate the buffer

        num_search_letters++;

        trigger_refresh();

        return 0;
}

// Determine the number of bytes in the last UTF-8 character
int get_last_char_bytes(const char *str, int len)
{
        if (len == 0)
                return 0;

        int i = len - 1;
        while (i >= 0 && (str[i] & 0xC0) == 0x80) {
                i--;
        }
        return len - i;
}

// Remove the preceding character from the search text
int remove_from_search_text(void)
{
        if (num_search_letters == 0)
                return 0;

        // Determine the number of bytes to remove for the last character
        int last_char_bytes = get_last_char_bytes(search_text, num_search_bytes);
        if (last_char_bytes == 0)
                return 0;

        // Remove the character from the buffer
        num_search_bytes -= last_char_bytes;
        search_text[num_search_bytes] = '\0';

        num_search_letters--;

        trigger_refresh();

        return 0;
}

void apply_color_and_format(bool is_chosen, FileSystemEntry *entry, UISettings *ui,
                            bool is_playing)
{
        if (is_chosen) {
                current_search_entry = entry;

                if (entry->is_enqueued) {
                        apply_color(ui->colorMode,
                                    is_playing ? ui->theme.search_playing
                                               : ui->theme.search_enqueued,
                                    ui->color);

                        printf("\x1b[7m * ");
                } else {
                        printf("  \x1b[7m ");
                }
        } else {
                if (entry->is_enqueued) {
                        apply_color(ui->colorMode,
                                    is_playing ? ui->theme.search_playing
                                               : ui->theme.search_enqueued,
                                    ui->color);

                        printf(" * ");

                        if (is_playing) {
                                printf("\e[4m");
                        }
                } else {
                        if (entry->parent != NULL && entry->parent->parent == NULL) {
                                apply_color(ui->colorMode,
                                            is_playing ? ui->theme.search_playing
                                                       : ui->theme.library_artist,
                                            ui->color);
                        }
                        printf("   ");
                }
        }
}

FileSystemEntry *last_directory = NULL;

int display_search_results(int row, int col, int max_list_size, int *chosen_row,
                           int start_search_iter)
{
        AppState *state = get_app_state();

        int term_w, term_h;
        get_term_size(&term_w, &term_h);

        int max_name_width = term_w - col - 5;
        if (!state->uiSettings.hideSideCover)
                max_name_width -= col /4;
        char name[max_name_width + 1];
        int printed_rows = 0;

        if (*chosen_row >= (int)results_count - 1) {
                *chosen_row = results_count - 1;
        }

        if (start_search_iter < 0)
                start_search_iter = 0;

        if (*chosen_row > start_search_iter + round(max_list_size / 2)) {
                start_search_iter = *chosen_row - round(max_list_size / 2) + 1;
        }

        if (*chosen_row < start_search_iter)
                start_search_iter = *chosen_row;

        if (*chosen_row < 0)
                start_search_iter = *chosen_row = 0;

        int name_width = max_name_width;
        int extra_indent = 0;
        UISettings *ui = &(state->uiSettings);

        for (size_t i = start_search_iter; i < results_count; i++) {
                if ((int)i >= max_list_size + start_search_iter - 1)
                        break;

                apply_color(ui->colorMode, ui->theme.search_result,
                            ui->defaultColorRGB);

                // Indent sub dirs
                if (results[i].parent != NULL)
                        extra_indent = 2;
                else if (!results[i].entry->is_directory &&
                         (last_directory != NULL) && results[i].entry->parent_id == last_directory->id)
                        extra_indent = 4;
                else
                        extra_indent = 0;

                name_width = max_name_width - extra_indent;

                printf("\033[%d;%dH", row, col);
                clear_rest_of_line();

                printf("\033[%d;%dH", row, col + extra_indent);

                bool is_chosen = (*chosen_row == (int)i);

                Node *current = get_current_song();

                bool isCurrentSong =
                    current != NULL && strcmp(current->song.file_path,
                                              results[i].entry->full_path) == 0;

                apply_color_and_format(is_chosen, results[i].entry, ui,
                                       isCurrentSong);

                name[0] = '\0';
                if (results[i].entry->is_directory) {
                        last_directory = results[i].entry;

                        if (results[i].entry->parent != NULL && results[i].entry->parent->parent == NULL) {
                                char *upper_dir_name = string_to_upper(results[i].entry->name);

                                snprintf(name, name_width + 1, "%s",
                                         upper_dir_name);

                                free(upper_dir_name);
                        } else {
                                snprintf(name, name_width + 1, "[%s]",
                                         results[i].entry->name);
                        }
                } else {
                        snprintf(name, name_width + 1, "%s [%s]",
                                 results[i].entry->name,
                                 (results[i].entry->parent != NULL ? results[i].entry->parent->name : "Root"));
                }
                printf("%s", name);
                row++;
                printed_rows++;
        }

        apply_color(ui->colorMode, ui->theme.help, ui->defaultColorRGB);

        while (printed_rows < max_list_size) {
                printf("\033[%d;%dH", row++, col);
                clear_rest_of_line(),
                    printed_rows++;
        }

        return 0;
}

int display_search(int row, int col, int max_list_size, int *chosen_row, int start_search_iter)
{
        display_search_box(row, col);
        printf("\033[%d;%dH", row+1, col);
        clear_rest_of_line();
        display_search_results(row + 2, col, max_list_size, chosen_row, start_search_iter);

        return 0;
}
