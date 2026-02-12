/**
 * @file control_ui.h
 * @brief Handles playback control interface rendering and input.
 *
 * Draws the transport controls (play/pause, skip, seek) and
 * maps user actions to playback operations.
 */

#include <stdbool.h>

/**
 * @brief Seeks forward in the current song by one step.
 *
 * This function moves the playback position forward by a fixed percentage
 * of the song's total duration. The amount of the seek is determined by the
 * number of progress bars in the UI. It also sets the state of fast-forwarding.
 */
void seek_forward(void);

/**
 * @brief Seeks backward in the current song by one step.
 *
 * This function moves the playback position backward by a fixed percentage
 * of the song's total duration. The amount of the seek is determined by the
 * number of progress bars in the UI. It also sets the state of rewinding.
 */
void seek_back(void);

/**
 * @brief Cycles through different color modes for the UI.
 *
 * The function toggles between three color modes: default, album-based,
 * and theme-based. It also ensures that the corresponding theme is loaded
 * for the selected color mode.
 */
void cycle_color_mode(void);

/**
 * @brief Cycles through the available themes.
 *
 * This function loads and applies the next available theme from the theme
 * directory. It wraps around to the first theme if the current one is the last.
 */
void cycle_themes(void);

/**
 * @brief Toggles the visibility of the lyrics page.
 *
 * This function toggles the UI state to show or hide the lyrics page.
 */
void toggle_show_lyrics_page(void);

/**
 * @brief Toggles the use of ASCII art for album covers.
 *
 * This function toggles between using ASCII art or the regular album covers
 * for the UI. It also stops the current visualizer if active.
 */
void toggle_ascii(void);

/**
 * @brief Toggles the visualizer on or off.
 *
 * This function toggles the state of the visualizer, stopping it if it
 * is currently running, or starting it if it is off.
 */
void toggle_visualizer(void);

/**
 * @brief Toggles the shuffle mode for playlist playback.
 *
 * This function toggles shuffle mode on or off, and modifies the current
 * playlist to match the new shuffle state.
 */
void toggle_shuffle(void);

/**
 * @brief Toggles the repeat mode for playback.
 *
 * This function cycles through the different repeat modes: none, list, or track.
 * The repeat state is updated accordingly.
 */
void toggle_repeat(void);

/**
 * @brief Pauses or unpauses the playback.
 *
 * If playback is stopped, it enqueues the current song for playback. If
 * playback is already active, it toggles the pause state.
 */
void toggle_pause(void);

/**
 * @brief Toggles notifications on or off.
 *
 * This function toggles the state of notifications in the UI, enabling or
 * disabling them as appropriate.
 */
void toggle_notifications(void);

/**
 * @brief Determines whether the player needs to be refreshed.
 *
 * This function checks if the player should be refreshed based on various
 * conditions, including skipping, EOF, and implementation switch states.
 *
 * @return true if the player needs to be refreshed, false otherwise.
 */
bool should_refresh_player(void);

/**
 * @brief Loads the specified theme.
 *
 * This function loads a theme file from the themes directory. It supports
 * both ANSI and Truecolor themes, and applies the theme to the UI settings.
 *
 * @param theme_name The name of the theme to be loaded.
 * @param is_ansi_theme Boolean flag to indicate if the theme is ANSI-based.
 *
 * @return 1 if the theme was successfully loaded, 0 if loading failed.
 */
int load_theme(const char *theme_name, bool is_ansi_theme);
