/**
 * @file search_ops.c
 * @brief Search interface for tracks and artists.
 *
 * Provides logic for querying the music library, filtering results,
 * and adding songs to playlists from search results.
 */

#include "search_ops.h"

#include "common/appstate.h"
#include "common/model.h"

#include "data/directorytree.h"
#include "data/playlist.h"

#include "utils/utils.h"

#include <stdbool.h>

#define MAX_SEARCH_LEN 32

// Global variables to store results

int results_capacity = 0;
int terminal_height = 0;

#define GROW_MARGIN 50

void realloc_results()
{
        Model *model = get_model();

        if (model->state.ui.search_results_count >= results_capacity) {
                results_capacity = results_capacity == 0
                                       ? 10 + GROW_MARGIN
                                       : results_capacity + GROW_MARGIN;

                model->search_results =
                    realloc(model->search_results, results_capacity * sizeof(SearchResult));
        }
}

void set_result_fields(FileSystemEntry *entry, int distance,
                       FileSystemEntry *parent)
{
        Model *model = get_model();

        model->search_results[model->state.ui.search_results_count].distance = distance;
        model->search_results[model->state.ui.search_results_count].entry = entry;
        model->search_results[model->state.ui.search_results_count].parent = parent;
        model->search_results[model->state.ui.search_results_count].num_children = 0;
}

bool is_duplicate(const FileSystemEntry *entry)
{
        Model *model = get_model();

        for (int i = 0; i < model->state.ui.search_results_count; i++) {
                const FileSystemEntry *other = model->search_results[i].entry;

                if (!entry->is_directory)
                        return false;

                if (entry == other)
                        return true;
        }

        return false;
}

int add_result_dir_contents(FileSystemEntry *entry, int distance)
{
        Model *model = get_model();
        int num_children = 0;
        int id = model->state.ui.search_results_count;

        if (entry->is_directory) {
                if (entry->children && entry->parent != NULL) {
                        FileSystemEntry *child = entry->children;

                        while (child) {
                                model->state.ui.search_results_count++;
                                realloc_results();
                                set_result_fields(child, distance, entry);

                                if (!child->is_directory)
                                        num_children++;

                                num_children += add_result_dir_contents(child, distance);

                                child = child->next;
                        }
                }

                model->search_results[id].num_children = num_children;
        }

        return num_children;
}

static int num_search_letters = 0;
static int num_search_bytes = 0;
static int min_search_letters = 1;

void add_result(FileSystemEntry *entry, int distance)
{
        Model *model = get_model();

        if (num_search_letters < min_search_letters)
                return;

        if (model->state.ui.search_results_count > terminal_height * 10)
                return;

        if (entry->parent == NULL) // Root
                return;

        if (is_duplicate(entry))
                return;

        realloc_results();
        set_result_fields(entry, distance, NULL);
        add_result_dir_contents(entry, distance);
        model->state.ui.search_results_count++;
}

void collect_result(FileSystemEntry *entry, int distance)
{
        add_result(entry, distance);
}

void search_shutdown(void)
{
        Model *model = get_model();

        if (model->search_results != NULL) {
                model->search_results->entry = NULL;
                model->search_results->parent = NULL;
                free(model->search_results);
                model->search_results = NULL;
        }

        if (model->state.ui.current_search_entry != NULL)
                model->state.ui.current_search_entry = NULL;

        results_capacity = 0;
        model->state.ui.search_results_count = 0;
        model->state.ui.chosen_search_result_row = 0;
}

void calculate_group_distances(void)
{
        Model *model = get_model();

        for (int i = 0; i < model->state.ui.search_results_count; i++) {
                // Find top-level parent (entry with no parent, or root)
                FileSystemEntry *root = model->search_results[i].entry;
                while (root->parent != NULL) {
                        root = root->parent;
                }

                // Find if this root appears in results, and use ITS distance
                // Otherwise use minimum distance among descendants
                int minDist = model->search_results[i].distance;

                for (int j = 0; j < model->state.ui.search_results_count; j++) {
                        FileSystemEntry *otherRoot = model->search_results[j].entry;
                        while (otherRoot->parent != NULL) {
                                otherRoot = otherRoot->parent;
                        }

                        if (otherRoot == root) {
                                if (model->search_results[j].entry == root) {
                                        // The root itself is in results - use
                                        // its distance
                                        minDist = model->search_results[j].distance;
                                        break;
                                }
                                if (model->search_results[j].distance < minDist) {
                                        minDist = model->search_results[j].distance + 1;
                                }
                        }
                }

                // If root is in results, use only its distance for grouping
                // Otherwise use the best child distance
                model->search_results[i].groupDistance = minDist;
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
        // if (A->entry->is_directory != B->entry->is_directory)
        //         return A->entry->is_directory ? -1 : 1;

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
        Model *model = get_model();

        calculate_group_distances();

        if (!model->search_results)
                return;

        qsort(model->search_results, model->state.ui.search_results_count, sizeof(SearchResult), compare_results);
}

void fuzzy_search(char *search_term, FileSystemEntry *root, int threshold)
{
        Model *model = get_model();
        terminal_height = model->term_h;

        search_shutdown();

        if (num_search_letters > min_search_letters) {
                fuzzy_search_recursive(root, search_term, threshold,
                                       collect_result);
        }

        sort_search_results();

        set_dirty(DIRTY_SEARCH);
}

void reset_search_result(Model *model)
{
        model->state.ui.chosen_search_result_row = 0;
}

int add_to_search_text(Model *model, const char *str)
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
                model->state.ui.search_text[num_search_bytes++] = str[i];
        }

        model->state.ui.search_text[num_search_bytes] = '\0'; // Null-terminate the buffer

        num_search_letters++;

        set_dirty(DIRTY_SEARCH);

        return 0;
}

int remove_from_search_text(Model *model)
{
        if (num_search_letters == 0)
                return 0;

        // Determine the number of bytes to remove for the last character
        int last_char_bytes = get_last_char_bytes(model->state.ui.search_text, num_search_bytes);
        if (last_char_bytes == 0)
                return 0;

        // Remove the character from the buffer
        num_search_bytes -= last_char_bytes;
        model->state.ui.search_text[num_search_bytes] = '\0';

        num_search_letters--;

        set_dirty(DIRTY_SEARCH);

        return 0;
}


