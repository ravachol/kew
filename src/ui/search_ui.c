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

typedef struct SearchResult
{
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

FileSystemEntry *get_current_search_entry(void) { return current_search_entry; }

int get_search_results_count(void) { return results_count; }

#define GROW_MARGIN 50

void realloc_results()
{
        if (results_count >= results_capacity)
        {
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
        for (size_t i = 0; i < results_count; i++)
        {
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

        if (entry->is_directory)
        {
                if (entry->children && entry->parent != NULL &&
                    entry->parent->parent == NULL)
                {
                        FileSystemEntry *child = entry->children;

                        while (child)
                        {
                                if (child->is_directory)
                                {
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
        if (results != NULL)
        {
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
        for (size_t i = 0; i < results_count; i++)
        {
                // Find top-level parent (entry with no parent, or root)
                FileSystemEntry *root = results[i].entry;
                while (root->parent != NULL)
                {
                        root = root->parent;
                }

                // Find if this root appears in results, and use ITS distance
                // Otherwise use minimum distance among descendants
                int minDist = results[i].distance;

                for (size_t j = 0; j < results_count; j++)
                {
                        FileSystemEntry *otherRoot = results[j].entry;
                        while (otherRoot->parent != NULL)
                        {
                                otherRoot = otherRoot->parent;
                        }

                        if (otherRoot == root)
                        {
                                if (results[j].entry == root)
                                {
                                        // The root itself is in results - use
                                        // its distance
                                        minDist = results[j].distance;
                                        break;
                                }
                                if (results[j].distance < minDist)
                                {
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
        if (A->entry->parent != B->entry->parent)
        {
                const FileSystemEntry *pA = A->entry;
                const FileSystemEntry *pB = B->entry;

                // Walk up to same depth
                int depthA = 0, depthB = 0;
                for (const FileSystemEntry *p = pA; p->parent; p = p->parent)
                        depthA++;
                for (const FileSystemEntry *p = pB; p->parent; p = p->parent)
                        depthB++;

                while (depthA > depthB)
                {
                        pA = pA->parent;
                        depthA--;
                }
                while (depthB > depthA)
                {
                        pB = pB->parent;
                        depthB--;
                }

                // Walk up together to find where they diverge
                while (pA->parent != pB->parent)
                {
                        pA = pA->parent;
                        pB = pB->parent;
                }

                // Compare by name at divergence point
                int cmp = strcmp(pA->name, pB->name);
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

        if (num_search_letters > min_search_letters)
        {
                fuzzy_search_recursive(root, search_text, threshold,
                                     collect_result);
        }

        sort_search_results();

        trigger_refresh();
}

int display_search_box(int indent)
{
        AppState *state = get_app_state();
        UISettings *ui = &(state->uiSettings);

        apply_color(ui->colorMode, ui->theme.search_label, ui->color);

        clear_line();
        print_blank_spaces(indent);
        printf(_(" [Search]: "));
        apply_color(ui->colorMode, ui->theme.search_query, ui->defaultColorRGB);
        // Save cursor position
        printf("%s", search_text);
        printf("\033[s");
        printf("█\n");

        return 0;
}

int add_to_search_text(const char *str)
{
        if (str == NULL)
        {
                return -1;
        }

        size_t len = strnlen(str, MAX_SEARCH_LEN);

        // Check if the string can fit into the search text buffer
        if (num_search_letters + len > MAX_SEARCH_LEN)
        {
                return 0; // Not enough space
        }

        // Add the string to the search text buffer
        for (size_t i = 0; i < len; i++)
        {
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
        while (i >= 0 && (str[i] & 0xC0) == 0x80)
        {
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
        int lastCharBytes = get_last_char_bytes(search_text, num_search_bytes);
        if (lastCharBytes == 0)
                return 0;

        // Remove the character from the buffer
        num_search_bytes -= lastCharBytes;
        search_text[num_search_bytes] = '\0';

        num_search_letters--;

        trigger_refresh();

        return 0;
}

void apply_color_and_format(bool isChosen, FileSystemEntry *entry, UISettings *ui,
                         bool is_playing)
{
        if (isChosen)
        {
                current_search_entry = entry;

                if (entry->isEnqueued)
                {
                        apply_color(ui->colorMode,
                                   is_playing ? ui->theme.search_playing
                                             : ui->theme.search_enqueued,
                                   ui->color);

                        printf("\x1b[7m * ");
                }
                else
                {
                        printf("  \x1b[7m ");
                }
        }
        else
        {
                if (entry->isEnqueued)
                {
                        apply_color(ui->colorMode,
                                   is_playing ? ui->theme.search_playing
                                             : ui->theme.search_enqueued,
                                   ui->color);

                        printf(" * ");
                }
                else
                {
                        if (entry->parent != NULL && entry->parent->parent == NULL)
                        {
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

int display_search_results(int max_list_size, int indent, int *chosen_row,
                         int start_search_iter)
{
        int term_w, term_h;
        get_term_size(&term_w, &term_h);

        int maxNameWidth = term_w - indent - 5;
        char name[maxNameWidth + 1];
        int printedRows = 0;

        if (*chosen_row >= (int)results_count - 1)
        {
                *chosen_row = results_count - 1;
        }

        if (start_search_iter < 0)
                start_search_iter = 0;

        if (*chosen_row > start_search_iter + round(max_list_size / 2))
        {
                start_search_iter = *chosen_row - round(max_list_size / 2) + 1;
        }

        if (*chosen_row < start_search_iter)
                start_search_iter = *chosen_row;

        if (*chosen_row < 0)
                start_search_iter = *chosen_row = 0;

        clear_line();
        printf("\n");
        printedRows++;

        int nameWidth = maxNameWidth;
        int extraIndent = 0;
        AppState *state = get_app_state();
        UISettings *ui = &(state->uiSettings);

        for (size_t i = start_search_iter; i < results_count; i++)
        {
                if ((int)i >= max_list_size + start_search_iter - 1)
                        break;

                apply_color(ui->colorMode, ui->theme.search_result,
                           ui->defaultColorRGB);

                clear_line();

                // Indent sub dirs
                if (results[i].parent != NULL)
                        extraIndent = 2;
                else if (!results[i].entry->is_directory &&
                        (last_directory != NULL) && results[i].entry->parentId == last_directory->id)
                        extraIndent = 4;
                else
                        extraIndent = 0;

                nameWidth = maxNameWidth - extraIndent;

                print_blank_spaces(indent + extraIndent);

                bool isChosen = (*chosen_row == (int)i);

                Node *current = get_current_song();

                bool isCurrentSong =
                    current != NULL && strcmp(current->song.filePath,
                                              results[i].entry->fullPath) == 0;

                apply_color_and_format(isChosen, results[i].entry, ui,
                                    isCurrentSong);

                name[0] = '\0';
                if (results[i].entry->is_directory)
                {
                        last_directory = results[i].entry;

                        if (results[i].entry->parent != NULL && results[i].entry->parent->parent == NULL)
                        {
                                char *upperDirName = string_to_upper(results[i].entry->name);

                                snprintf(name, nameWidth + 1, "%s",
                                        upperDirName);


                                free(upperDirName);
                        }
                        else {
                                snprintf(name, nameWidth + 1, "[%s]",
                                        results[i].entry->name);
                        }
                }
                else
                {
                        snprintf(name, nameWidth + 1, "%s [%s]",
                                 results[i].entry->name,
                                 (results[i].entry->parent != NULL ?  results[i].entry->parent->name : "Root"));
                }
                printf("%s\n", name);
                printedRows++;
        }

        apply_color(ui->colorMode, ui->theme.help, ui->defaultColorRGB);

        while (printedRows < max_list_size)
        {
                clear_line();
                printf("\n");
                printedRows++;
        }

        return 0;
}

int display_search(int max_list_size, int indent, int *chosen_row,
                  int start_search_iter)
{
        display_search_box(indent);
        display_search_results(max_list_size, indent, chosen_row, start_search_iter);

        return 0;
}
