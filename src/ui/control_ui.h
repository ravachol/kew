/**
 * @file control_ui.[h]
 * @brief Handles playback control interface rendering and input.
 *
 * Draws the transport controls (play/pause, skip, seek) and
 * maps user actions to playback operations.
 */

#include "common/appstate.h"

void seekForward(UIState *uis);

void seekBack(UIState *uis);

void cycleColorMode(AppState *state);

void cycleThemes(AppState *state, AppSettings *settings);

void toggleShowLyricsPage(AppState *state);

void toggleAscii(AppSettings *settings, UISettings *ui);

void toggleVisualizer(AppSettings *settings, UISettings *ui);

void toggleShuffle(AppState *state);

void toggleNotifications(UISettings *ui, AppSettings *settings);

void toggleRepeat(AppState *state);

int loadTheme(AppState *appState, AppSettings *settings, const char *themeName, bool isAnsiTheme);

bool shouldRefreshPlayer(void);
