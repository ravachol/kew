/**
 * @file search_ops.h
 * @brief Search interface for tracks and artists.
 *
 * Provides logic for querying the music library, filtering results,
 * and adding songs to playlists from search results.
 */

#include "common/model.h"
#include "data/directorytree.h"

/**
 * @brief Adds a string to the search text.
 *
 * This function appends a string to the current search text buffer. If there is not enough
 * space in the buffer, it will return without modifying the search text.
 *
 * @param model
 * @param str The string to add to the search text.
 * @return Returns 0 on success, or -1 if the string is NULL.
 */
int add_to_search_text(Model *model, const char *str);

/**
 * @brief Resets the search results.
 *
 * @param model
 */
void reset_search_result(Model *model);

/**
 * @brief Removes the last character from the search text.
 *
 * This function removes the preceding character from the search text, updating the buffer
 * and adjusting the number of search letters accordingly.
 *
 * @param model
 *
 * @return Returns 0 on success.
 */
int remove_from_search_text(Model *model);

/**
 * @brief Sets the chosen directory for search results.
 *
 * This function sets the directory entry as the chosen directory for the search view.
 *
 * @param entry The FileSystemEntry that represents the chosen directory.
 */
void set_chosen_search_dir(FileSystemEntry *entry);

/**
 * @brief Frees the memory allocated for the search results.
 *
 * This function frees the memory used by the search results and resets related variables.
 */
void free_search_results(void);

/**
 * @brief Performs a fuzzy search on the file system entries.
 *
 * This function performs a fuzzy search starting from the provided root entry, with the given
 * threshold value for distance. The search results are collected during the process.
 *
 * @param search_term
 * @param root The root entry to begin the search from.
 * @param threshold The maximum allowed fuzzy search distance.
 */
void fuzzy_search(char *search_term, FileSystemEntry *root, int threshold);

/**
 * @brief Gets the chosen search directory.
 *
 * This function returns the directory entry that has been selected as the chosen directory
 * for the search results.
 *
 * @return The chosen FileSystemEntry that represents the selected search directory.
 */
FileSystemEntry *get_chosen_search_dir(void);

/**
 * @brief Displays a search interface, including the search box and results.
 *
 * This function handles the display of the search box, clears the remaining line,
 * and then displays the search results starting from the specified position on the screen.
 * It manages the layout of the search interface and updates the screen accordingly.
 *
 * @param row The row position where the search interface will be displayed.
 * @param col The column position where the search interface will be displayed.
 * @param max_list_size The maximum number of search results to display.
 * @param chosen_row A pointer to the currently chosen row in the search results.
 *
 * @return Returns 0 when the function completes successfully.
 */
int display_search(int row, int col, int max_list_size, int *chosen_row);
