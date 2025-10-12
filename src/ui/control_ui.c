/**
 * @file control_ui.[c]
 * @brief Handles playback control interface rendering and input.
 *
 * Draws the transport controls (play/pause, skip, seek) and
 * maps user actions to playback operations.
 */

#include "control_ui.h"

#include "ops/playback_ops.h"
#include "ops/playback_state.h"

#include "common/common.h"

#include "sys/mpris.h"

#include "data/theme.h" // FIXME: should really only be talking top ops, not data directly

#include "utils/term.h"
#include "utils/utils.h"

#include <miniaudio.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

void seekForward(UIState *uis)
{
        Node *current = getCurrentSong();
        if (current == NULL)
                return;

        double duration = current->song.duration;
        if (duration <= 0.0)
                return;

        double stepPercent = 100.0 / uis->numProgressBars;

        int seconds = (int)(duration * (stepPercent / 100.0));

        playbackSeekForward(seconds);

        uis->isRewinding = true;
}

void seekBack(UIState *uis)
{
        Node *current = getCurrentSong();
        if (current == NULL)
                return;

        double duration = current->song.duration;
        if (duration <= 0.0)
                return;

        double stepPercent = 100.0 / uis->numProgressBars;

        int seconds = (int)(duration * (stepPercent / 100.0));

        playbackSeekBack(seconds);

        uis->isRewinding = true;
}

void cycleColorMode(AppState *state)
{
        UISettings *ui = &(state->uiSettings);
        AppSettings *settings = getAppSettings();

        clearScreen();

        switch (ui->colorMode)
        {
        case COLOR_MODE_DEFAULT:
                ui->colorMode = COLOR_MODE_ALBUM;
                break;
        case COLOR_MODE_ALBUM:
                ui->colorMode = COLOR_MODE_THEME;
                break;
        case COLOR_MODE_THEME:
                ui->colorMode = COLOR_MODE_DEFAULT;
                break;
        }

        bool themeLoaded = false;

        switch (ui->colorMode)
        {
        case COLOR_MODE_DEFAULT:
                if (loadTheme(state, settings, "default", true))
                {
                        themeLoaded = true;
                }
                break;
        case COLOR_MODE_ALBUM:
                themeLoaded = true;
                break;
        case COLOR_MODE_THEME:
                if (ui->themeName[0] != '\0' &&
                    loadTheme(state, settings, ui->themeName, true))
                {
                        themeLoaded = true;
                }
        }

        if (!themeLoaded)
        {
                cycleColorMode(state);
        }

        triggerRefresh();
}

void cycleThemes(AppState *state, AppSettings *settings)
{
        clearScreen();

        UISettings *ui = &(state->uiSettings);

        char *configPath = getConfigPath();
        if (!configPath)
                return;

        char themesPath[MAXPATHLEN];
        snprintf(themesPath, sizeof(themesPath), "%s/themes", configPath);

        DIR *dir = opendir(themesPath);
        if (!dir)
        {
                perror("Failed to open themes directory");
                return;
        }

        struct dirent *entry;
        char *themes[256];
        int themeCount = 0;

        // Collect all *.theme files
        while ((entry = readdir(dir)) != NULL)
        {
                if (strstr(entry->d_name, ".theme"))
                {
                        themes[themeCount++] = strdup(entry->d_name);
                }
        }
        closedir(dir);

        if (themeCount == 0)
        {
                setErrorMessage("No themes found.");
                free(configPath);
                return;
        }

        // Find the index of the current theme
        int currentIndex = -1;
        char currentFilename[NAME_MAX];

        strncpy(currentFilename, ui->themeName, sizeof(currentFilename) - 1);
        currentFilename[sizeof(currentFilename) - 1] = '\0';

        if (strlen(currentFilename) + 6 < sizeof(currentFilename))
                strcat(currentFilename, ".theme");

        for (int i = 0; i < themeCount; i++)
        {
                if (strcmp(themes[i], currentFilename) == 0)
                {
                        currentIndex = i;
                        break;
                }
        }

        // Get next theme (wrap around)
        int nextIndex = (currentIndex + 1) % themeCount;

        if (loadTheme(state, settings, themes[nextIndex], false))
        {
                ui->colorMode = COLOR_MODE_THEME;

                snprintf(ui->themeName, sizeof(ui->themeName), "%s",
                         themes[nextIndex]);
                char *dot = strstr(ui->themeName, ".theme");
                if (dot)
                        *dot = '\0';
        }

        triggerRefresh();

        for (int i = 0; i < themeCount; i++)
        {
                free(themes[i]);
        }

        free(configPath);
}

void toggleVisualizer(AppSettings *settings, UISettings *ui)
{
        ui->visualizerEnabled = !ui->visualizerEnabled;
        c_strcpy(settings->visualizerEnabled, ui->visualizerEnabled ? "1" : "0",
                 sizeof(settings->visualizerEnabled));
        restoreCursorPosition();
        triggerRefresh();
}

void toggleShowLyricsPage(AppState *state)
{
        state->uiState.showLyricsPage = !state->uiState.showLyricsPage;
        triggerRefresh();
}

void toggleAscii(AppSettings *settings, UISettings *ui)
{
        ui->coverAnsi = !ui->coverAnsi;
        c_strcpy(settings->coverAnsi, ui->coverAnsi ? "1" : "0",
                 sizeof(settings->coverAnsi));

        if (ui->coverEnabled)
        {
                clearScreen();
                triggerRefresh();
        }
}

void toggleRepeat(AppState *state)
{

        bool repeatEnabled = playbackIsRepeatEnabled();
        bool repeatListEnabled = playbackIsRepeatListEnabled();

        if (repeatEnabled)
        {
                playbackSetRepeatEnabled(false);
                playbackSetRepeatListEnabled(true);
                emitStringPropertyChanged("LoopStatus", "List");
                state->uiSettings.repeatState = 2;
        }
        else if (repeatListEnabled)
        {
                playbackSetRepeatEnabled(false);
                playbackSetRepeatListEnabled(false);
                emitStringPropertyChanged("LoopStatus", "None");
                state->uiSettings.repeatState = 0;
        }
        else
        {
                playbackSetRepeatEnabled(true);
                playbackSetRepeatListEnabled(false);
                emitStringPropertyChanged("LoopStatus", "Track");
                state->uiSettings.repeatState = 1;
        }

        if (state->currentView != TRACK_VIEW)
                triggerRefresh();
}

void toggleNotifications(UISettings *ui, AppSettings *settings)
{
        ui->allowNotifications = !ui->allowNotifications;
        c_strcpy(settings->allowNotifications,
                 ui->allowNotifications ? "1" : "0",
                 sizeof(settings->allowNotifications));

        if (ui->allowNotifications)
        {
                clearScreen();
                triggerRefresh();
        }
}

void toggleShuffle(AppState *state)
{
        state->uiSettings.shuffleEnabled = !playbackIsShuffleEnabled();
        playbackSetShuffleEnabled(state->uiSettings.shuffleEnabled);

        Node *current = getCurrentSong();
        PlayList *playlist = getPlaylist();
        PlayList *unshuffledPlaylist = getUnshuffledPlaylist();

        if (state->uiSettings.shuffleEnabled)
        {
                pthread_mutex_lock(&(playlist->mutex));

                shufflePlaylistStartingFromSong(playlist, current);

                pthread_mutex_unlock(&(playlist->mutex));

                emitBooleanPropertyChanged("Shuffle", TRUE);
        }
        else
        {
                char *path = NULL;
                if (current != NULL)
                {
                        path = strdup(current->song.filePath);
                }

                pthread_mutex_lock(&(playlist->mutex));

                PlayList *playlist = getPlaylist();
                ;
                deepCopyPlayListOntoList(unshuffledPlaylist, &playlist);
                setPlaylist(playlist);

                if (path != NULL)
                {
                        setCurrentSong(findPathInPlaylist(path, playlist));
                        free(path);
                }

                pthread_mutex_unlock(&(playlist->mutex));

                emitBooleanPropertyChanged("Shuffle", FALSE);
        }

        state->uiState.loadedNextSong = false;
        setNextSong(NULL);

        if (state->currentView == PLAYLIST_VIEW ||
            state->currentView == LIBRARY_VIEW)
                triggerRefresh();
}

bool shouldRefreshPlayer(void)
{
        PlaybackState *ps = getPlaybackState();

        return !ps->skipping && !playbackIsEof() && !playbackIsImplSwitchReached();
}

void strToLower(char *str)
{
        if (str == NULL)
                return;

        for (; *str; ++str)
        {
                *str = tolower((unsigned char)*str);
        }
}

int loadTheme(AppState *appState, AppSettings *settings, const char *themeName,
              bool isAnsiTheme)
{
        if (!appState || !themeName)
                return 0;

        char *configPath = getConfigPath();
        if (!configPath)
                return 0;

        // Check if config directory exists
        struct stat st = {0};
        if (stat(configPath, &st) == -1)
        {
                free(configPath);
                return 0;
        }

        // Build full theme filename
        char filename[NAME_MAX];
        const char *extension = ".theme";
        size_t themeNameLen = strlen(themeName);
        size_t extLen = strlen(extension);

        // Check if themeName already ends with ".theme"
        int hasExtension =
            (themeNameLen >= extLen &&
             strcmp(themeName + themeNameLen - extLen, extension) == 0);

        if (hasExtension)
        {
                if (snprintf(filename, sizeof(filename), "%s", themeName) >=
                    (int)sizeof(filename))
                {
                        fprintf(stderr, "Theme filename is too long\n");
                        setErrorMessage("Theme filename is too long");
                        free(configPath);
                        return 0;
                }
        }
        else
        {
                if (snprintf(filename, sizeof(filename), "%s.theme",
                             themeName) >= (int)sizeof(filename))
                {
                        fprintf(stderr, "Theme filename is too long\n");
                        setErrorMessage("Theme filename is too long");
                        free(configPath);
                        return 0;
                }
        }

        // Build full themes directory path: configDir + "/themes"
        char themesDir[MAXPATHLEN];
        if (snprintf(themesDir, sizeof(themesDir), "%s/themes", configPath) >=
            (int)sizeof(themesDir))
        {
                fprintf(stderr, "Themes path is too long\n");
                setErrorMessage("Themes path is too long");
                free(configPath);
                return 0;
        }

        strToLower(filename);

        // Call the loader
        int loaded =
            loadThemeFromFile(themesDir, filename, &appState->uiSettings.theme);
        if (!loaded)
        {
                free(configPath);
                return 0; // failed to load
        }

        appState->uiSettings.themeIsSet = true;

        if (isAnsiTheme)
        {
                // Default ANSI theme: store in settings->ansiTheme
                snprintf(settings->ansiTheme, sizeof(settings->ansiTheme), "%s",
                         themeName);
        }
        else
        {
                // Truecolor theme: store in settings->theme
                snprintf(settings->theme, sizeof(settings->theme), "%s",
                         themeName);
        }

        free(configPath);

        return 1;
}
