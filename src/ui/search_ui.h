/**
 * @file search_ui.h
 * @brief Search interface for tracks and artists.
 *
 * Provides UI and logic for querying the music library, filtering results,
 * and adding songs to playlists from search results.
 */

#include "data/directorytree.h"

int display_search(int row, int col, int max_list_size, int *chosen_row, int start_search_iter);
int add_to_search_text(const char *str);
int remove_from_search_text(void);
int get_search_results_count(void);
void free_search_results(void);
void fuzzy_search(FileSystemEntry *root, int threshold);
FileSystemEntry *get_current_search_entry(void);
