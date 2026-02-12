/**
 * @file search_ui.h
 * @brief Search interface for tracks and artists.
 *
 * Provides UI and logic for querying the music library, filtering results,
 * and adding songs to playlists from search results.
 */

#include "data/directorytree.h"

/**
 * @brief Displays the search box with the current search text.
 *
 * This function renders the search box on the screen at the specified row and column,
 * including the current search text input by the user.
 *
 * @param row The row at which the search box is displayed.
 * @param col The column at which the search box is displayed.
 * @return Returns 0 on success.
 */
int display_search_box(int row, int col);

/**
 * @brief Adds a string to the search text.
 *
 * This function appends a string to the current search text buffer. If there is not enough
 * space in the buffer, it will return without modifying the search text.
 *
 * @param str The string to add to the search text.
 * @return Returns 0 on success, or -1 if the string is NULL.
 */
int add_to_search_text(const char *str);

/**
 * @brief Removes the last character from the search text.
 *
 * This function removes the preceding character from the search text, updating the buffer
 * and adjusting the number of search letters accordingly.
 *
 * @return Returns 0 on success.
 */
int remove_from_search_text(void);

/**
 * @brief Retrieves the count of search results.
 *
 * This function returns the total number of search results currently available.
 *
 * @return The number of search results.
 */
int get_search_results_count(void);

/**
 * @brief Checks if the search results have reached the last row.
 *
 * This function determines whether the current row of results is the last row in the search
 * result list.
 *
 * @return Returns true if the last row has been reached, false otherwise.
 */
bool is_at_last_row(void);

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
 * @param root The root entry to begin the search from.
 * @param threshold The maximum allowed fuzzy search distance.
 */
void fuzzy_search(FileSystemEntry *root, int threshold);

/**
 * @brief Gets the current search entry.
 *
 * This function returns the current search entry that is selected or being highlighted in the UI.
 *
 * @return The currently selected FileSystemEntry in the search results.
 */
FileSystemEntry *get_current_search_entry(void);

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

