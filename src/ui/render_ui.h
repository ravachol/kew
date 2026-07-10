/**
 * @file render_ui.h
 * @brief Main player screen rendering.
 *
 * Displays current track info, progress bar, and playback status.
 * Acts as the central visual component of the terminal player.
 */

#ifndef PLAYER_H
#define PLAYER_H

#include "common/model.h"
#include "common/appstate.h"

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
 * @brief Renders the kew player in the terminal
 *
 * @param model
 * @param ctx Contains rendering related information
 */
void render_ui(Model *model, RenderContext *ctx);

/**
 * @brief Resize the draw buffer to cols, rows size
 *
 * @param cols The number of rows.
 * @param rows The number of columns.
 */
void draw_buffer_resize(int cols, int rows);

/**
 * @brief Initializes the UI.
 *
 */
void ui_init(void);

/**
 * @brief Shuts down the UI.
 *
 * @param model
 */
void ui_shutdown(void);

/**
 * @brief Rebuilds the layout, and region sizes
 *
 * Should be called on for instance resize.
 *
 * @param model
 */
 void rebuild_layout(Model *model);

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
 * @brief Calculates the indentation value for a view.
 *
 * @return The calculated indentation value.
 */
void calc_indent(Model *model);

/**
 * @brief Prints the "About" information for the current version of the application.
 *
 * This function displays the "About" screen, including details about the current
 * version of the application and its components.
 *
 * @param model
 *
 * @return Returns 0 on success, or a non-zero value on error.
 */
int print_about_for_version(Model *model);

/**
 * @brief Displays the help screen for the application.
 *
 * This function shows the help documentation, providing users with information
 * on how to use the application.
 */
void print_help(void);

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
bool themes_init(int argc, char *argv[]);

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

char *url_at_pos(int row, int col);

#endif
