/**
 * @file player_ui.h
 * @brief Main player screen rendering.
 *
 * Displays current track info, progress bar, and playback status.
 * Acts as the central visual component of the terminal player.
 */

#ifndef PLAYER_H
#define PLAYER_H

#include "common/appstate.h"

#include "data/directorytree.h"

#include <stdbool.h>

/**
 * @brief Prints the logo art on the screen.
 *
 * This function displays the logo art at a specific location on the terminal, with
 * various options such as centering the logo, printing a tagline, and using a gradient.
 *
 * @param row The row position to start displaying the logo art.
 * @param col The column position to start displaying the logo art.
 * @param ui A pointer to the UI settings.
 * @param centered Whether to center the logo art or not.
 * @param print_tag_line Whether to print the tagline beneath the logo.
 * @param use_gradient Whether to use gradient coloring for the logo art.
 *
 * @return Returns 0 on success, or a non-zero value on error.
 */
int print_logo_art(int row, int col, const UISettings *ui, bool centered, bool print_tag_line, bool use_gradient);


/**
 * @brief Calculates the indentation value for normal text output.
 *
 * This function returns the standard indentation used in the output, typically
 * for aligning text to a specific column.
 *
 * @return The calculated indentation value.
 */
int calc_indent_normal(void);


/**
 * @brief Prints player information, including the song data and elapsed time.
 *
 * This function formats and prints information about the current song, such as
 * the title, artist, and elapsed time in the player interface.
 *
 * @param songdata A pointer to the song data to be printed.
 * @param elapsed_seconds The number of seconds that have elapsed since the song started.
 *
 * @return Returns 0 on success, or a non-zero value on error.
 */
int print_player(SongData *songdata, double elapsed_seconds);


/**
 * @brief Retrieves the row position for the footer in the UI.
 *
 * This function returns the row position where the footer should be displayed.
 *
 * @return The footer row position.
 */
int get_footer_row(void);


/**
 * @brief Retrieves the column position for the footer in the UI.
 *
 * This function returns the column position where the footer should be displayed.
 *
 * @return The footer column position.
 */
int get_footer_col(void);


/**
 * @brief Retrieves the indentation for text output.
 *
 * This function returns the current indentation used in the output, typically
 * for aligning text to a specific column.
 *
 * @return The indentation value.
 */
int get_indent(void);


/**
 * @brief Prints the "About" information for the current version of the application.
 *
 * This function displays the "About" screen, including details about the current
 * version of the application and its components.
 *
 * @param songdata A pointer to the song data, used to customize the about section.
 *
 * @return Returns 0 on success, or a non-zero value on error.
 */
int print_about_for_version(SongData *songdata);


/**
 * @brief Retrieves the currently chosen row in the UI.
 *
 * This function returns the row that is currently selected or focused in library view.
 *
 * @return The currently chosen row.
 */
int get_chosen_row(void);


/**
 * @brief Flips to the next page in the UI.
 *
 * This function switches the displayed content to the next page in the current view.
 */
void flip_next_page(void);


/**
 * @brief Flips to the previous page in the UI.
 *
 * This function switches the displayed content to the previous page in the current view.
 */
void flip_prev_page(void);


/**
 * @brief Displays the help screen for the application.
 *
 * This function shows the help documentation, providing users with information
 * on how to use the application.
 */
void show_help(void);


/**
 * @brief Sets the currently chosen directory from a file system entry.
 *
 * This function updates the UI to reflect the chosen directory based on the given
 * file system entry.
 *
 * @param entry A pointer to the file system entry representing the chosen directory.
 */
void set_chosen_dir(FileSystemEntry *entry);


/**
 * @brief Sets the current directory as the chosen directory.
 *
 * This function sets the current working directory to be the chosen directory in the UI.
 */
void set_current_as_chosen_dir(void);


/**
 * @brief Scrolls the UI to the next page of content.
 *
 * This function scrolls the displayed content down.
 */
void scroll_next(void);


/**
 * @brief Scrolls the UI to the previous page of content.
 *
 * This function scrolls the displayed content up.
 */
void scroll_prev(void);


/**
 * @brief Toggles the visibility of a specific view in the UI.
 *
 * This function switches between different views, showing or hiding the specified
 * view based on the current state.
 *
 * @param VIEW_TO_SHOW The view to be shown or hidden.
 */
void toggle_show_view(ViewState VIEW_TO_SHOW);


/**
 * @brief Displays information about the current track in the player.
 *
 * This function updates the display to show the track's title, artist, and other
 * related information in the player interface.
 */
void show_track(void);


/**
 * @brief Saves the current state of the media library.
 *
 * This function persists the current media library state, saving any changes made
 * to the library to disk or a database.
 */
void save_library(void);


/**
 * @brief Frees the resources associated with the media library.
 *
 * This function deallocates any memory and resources used by the media library,
 * effectively resetting the state of the library.
 */
void free_library(void);


/**
 * @brief Resets the chosen directory to its default state.
 *
 * This function clears the current directory selection and resets it to the
 * default directory.
 */
void reset_chosen_dir(void);


/**
 * @brief Switches to the next view in the UI.
 *
 * This function changes the UI to the next available view, cycling through
 * different UI modes.
 */
void switch_to_next_view(void);


/**
 * @brief Switches to the previous view in the UI.
 *
 * This function changes the UI to the previous available view, cycling through
 * different UI modes.
 */
void switch_to_previous_view(void);


/**
 * @brief Resets the search result to its default state.
 *
 * This function clears the current search result, effectively resetting the
 * search interface.
 */
void reset_search_result(void);


/**
 * @brief Sets the currently chosen row in the UI.
 *
 * This function updates the currently selected row to the specified value.
 *
 * @param row The new row to set as the chosen row.
 */
void set_chosen_row(int row);


/**
 * @brief Triggers a redraw of the side cover.
 *
 * This function forces the UI to refresh and redraw the side cover.
 */
void trigger_redraw_side_cover(void);


/**
 * @brief Refreshes the player interface.
 *
 * This function updates and redraws the player interface to reflect any changes.
 */
void refresh_player(void);


/**
 * @brief Sets the track title as the window title.
 *
 * This function updates the window title with the title of the currently playing track.
 */
void set_track_title_as_window_title(void);


/**
 * @brief Retrieves the file path for the library.
 *
 * This function returns the path to the media library file used by the application.
 *
 * @return The file path to the library.
 */
char *get_library_file_path(void);


/**
 * @brief Initializes the theme based on command-line arguments.
 *
 * This function loads and applies the appropriate theme for the application,
 * potentially altering the visual appearance based on the command-line arguments.
 *
 * @param argc The number of arguments passed to the application.
 * @param argv The array of arguments passed to the application.
 *
 * @return Returns true if the theme was successfully initialized, false otherwise.
 */
bool init_theme(int argc, char *argv[]);


/**
 * @brief Retrieves the currently chosen directory.
 *
 * This function returns a pointer to the file system entry representing the
 * currently chosen directory.
 *
 * @return A pointer to the chosen directory.
 */
FileSystemEntry *get_chosen_dir(void);


/**
 * @brief Requests the next visualization.
 *
 * This function triggers the next visualization to be displayed in the UI.
 */
void request_next_visualization(void);


/**
 * @brief Requests the stopping of the current visualization.
 *
 * This function halts the current visualization from being displayed.
 */
void request_stop_visualization(void);


/**
 * @brief Retrieves the footer text to display.
 *
 * This function fills the provided text buffer with the content to be displayed
 * in the footer.
 *
 * @param text A pointer to the text buffer to be filled with footer text.
 * @param size The size of the text buffer.
 *
 * @return Returns the length of the footer text written to the buffer.
 */
int get_footer_text(char *restrict text, size_t size);

#endif
