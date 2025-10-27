/**
 * @file control_ui.h
 * @brief Handles playback control interface rendering and input.
 *
 * Draws the transport controls (play/pause, skip, seek) and
 * maps user actions to playback operations.
 */

#include <stdbool.h>

void seek_forward(void);
void seek_back(void);
void cycle_color_mode(void);
void cycle_themes(void);
void toggle_show_lyrics_page(void);
void toggle_ascii(void);
void toggle_visualizer(void);
void toggle_shuffle(void);
void toggle_notifications(void);
void toggle_repeat(void);
void toggle_pause();
bool should_refresh_player(void);
int load_theme(const char *themeName, bool isAnsiTheme);

