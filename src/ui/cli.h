/**
 * @file cli.h
 * @brief Handles command-line related functions
 *
 * Contains the function that shows the welcome screen and sets the path for the first time.
 */

#include <stdbool.h>

/**
 * @brief Removes an element from argv array.
 *
 * @param[in,out] argv[] Array with arguments.
 * @param[in] index  Index of element to remove.
 * @param[in,out] argc   Argument count.
 */
void remove_arg_element(char *argv[], int index, int *argc);

/**
 * @brief Parses command-line options and updates application settings.
 *
 * This function scans the argument vector `argv` for known command-line
 * options and updates the corresponding fields in the application's
 * UI settings (`AppState::uiSettings`) as well as the `exact_search` flag.
 * Recognized options are removed from `argv`, and `*argc` is adjusted.
 *
 * Recognized options:
 *   - `--noui`        : disables the UI (`uiEnabled = false`)
 *   - `--nocover`     : disables cover display (`coverEnabled = false`)
 *   - `--quitonstop` / `-q` : enables quitting after stopping (`quitAfterStopping = true`)
 *   - `--exact` / `-e`      : sets `*exact_search = true`
 *
 * @param[in,out] argc          Pointer to the argument count; will be modified
 *                               if any options are removed.
 * @param[in,out] argv          Array of argument strings; recognized options
 *                               will be removed.
 * @param[out]    exact_search  Pointer to a boolean flag that will be set
 *                               if an exact search option is found.
 */
void handle_options(int *argc, char *argv[], bool *exact_search);

/**
 * @brief Displays a menu on screen that allows the user to set the path setting (path to music library).
 *
 */
void set_music_path(void);
