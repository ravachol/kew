/**
 * @file control_ui.h
 * @brief Handles playback control interface rendering and input.
 *
 * Draws the transport controls (play/pause, skip, seek) and
 * maps user actions to playback operations.
 */

#include <stdbool.h>

void seekForward(void);
void seekBack(void);
void cycleColorMode(void);
void cycleThemes(void);
void toggleShowLyricsPage(void);
void toggleAscii(void);
void toggleVisualizer(void);
void toggleShuffle(void);
void toggleNotifications(void);
void toggleRepeat(void);
void togglePause();
bool shouldRefreshPlayer(void);
int loadTheme(const char *themeName, bool isAnsiTheme);

