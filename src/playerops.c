#include "playerops.h"
#include "appstate.h"
#include "file.h"
#include "gio/gio.h"
#include "player_ui.h"
#include "search_ui.h"
#include "songloader.h"
#include "sound.h"
#include "common.h"
#include "soundcommon.h"
#include "term.h"
#include "theme.h"
#include "utils.h"
#include <ctype.h>
#include <dirent.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

/*

playerops.c

 Related to features/actions of the player.

*/

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

#ifndef ASK_IF_USE_CACHE_LIMIT_SECONDS
#define ASK_IF_USE_CACHE_LIMIT_SECONDS 4
#endif

#ifndef G_USEC_PER_SEC
#define G_USEC_PER_SEC 1000000
#endif

static LoadingThreadData loadingdata;
static struct timespec current_time;
static struct timespec startTime;
static struct timespec pauseTime;
static struct timespec lastUpdateTime = {0, 0};
static bool usingSongDataA = true;
static bool skipOutOfOrder = false;
static volatile bool songLoading = false;
static volatile bool clearingErrors = false;
static bool songHasErrors = false;
static int lastPlayedId = -1;
static bool skipping = false;
static bool skipFromStopped = false;
static bool nextSongNeedsRebuilding = false;
static bool forceSkip = false;
static double seekAccumulatedSeconds = 0.0;
static bool hasSilentlySwitched = false;
static double elapsedSeconds = 0.0;
static GDBusConnection *connection = NULL;
static GMainContext *globalMainContext = NULL;

typedef struct
{
        char *path;
        AppState *state;
} UpdateLibraryThreadArgs;

LoadingThreadData *getLoadingData() { return &loadingdata; }

bool isSkipOutOfOrder(void) { return skipOutOfOrder; }

void setSkipOutOfOrder(bool val) { skipOutOfOrder = val; }

bool isSongLoading(void) { return songLoading; }

void setSongLoading(bool val) { songLoading = val; }

bool hasErrorsSong(void) { return songHasErrors; }

struct timespec getPauseTime(void) { return pauseTime; }

int getLastPlayedId(void) { return lastPlayedId; }

void setLastPlayedId(int id) { lastPlayedId = id; }

void setGMainContext(GMainContext *val)
{
        globalMainContext = val;
}

void *getGMainContext(void)
{
        return globalMainContext;
}

void setGDBusConnection(GDBusConnection *val)
{
        connection = val;
}

GDBusConnection *getGDBusConnection(void)
{
        return connection;
}

double getElapsedSeconds(void)
{
        return elapsedSeconds;
}

bool shouldRefreshPlayer()
{
        return !skipping && !isEOFReached() && !isImplSwitchReached();
}

void setIsUsingSongDataA(bool val) { usingSongDataA = val; }

bool isUsingSongDataA() { return usingSongDataA; }

bool hasSkippedFromStopped(void) { return skipFromStopped; }

void setNextSongNeedsRebuilding(bool val) { nextSongNeedsRebuilding = val; }

bool getNextSongNeedsRebuilding(void) { return nextSongNeedsRebuilding; }

void handleSkipFromStopped(void)
{
        // If we don't do this the song gets loaded in the wrong slot
        if (skipFromStopped)
        {
                usingSongDataA = !usingSongDataA;
                skipOutOfOrder = false;
                skipFromStopped = false;
        }
}

void reshufflePlaylist(void)
{
        PlayList *playlist = getPlaylist();

        if (isShuffleEnabled())
        {
                Node *current = getCurrentSong();

                if (current != NULL)
                        shufflePlaylistStartingFromSong(playlist, current);
                else
                        shufflePlaylist(playlist);

                nextSongNeedsRebuilding = true;
        }
}

void skip(AppState *state)
{
        setCurrentImplementationType(NONE);

        setRepeatEnabled(false);

        AudioData *audioData = getAudioData();

        audioData->endOfListReached = false;

        if (!isPlaying())
        {
                switchAudioImplementation(state);
                skipFromStopped = true;
        }
        else
        {
                setSkipToNext(true);
        }

        if (!skipOutOfOrder)
                triggerRefresh();
}

void updatePlaybackPosition(double elapsedSeconds)
{
#ifndef __APPLE__
        if (elapsedSeconds < 0.0)
                elapsedSeconds = 0.0;

        // Max safe seconds to avoid overflow when multiplied by 1,000,000
        const double maxSeconds = (double)(LLONG_MAX / G_USEC_PER_SEC);

        if (elapsedSeconds > maxSeconds)
                elapsedSeconds = maxSeconds;

        GVariantBuilder changedPropertiesBuilder;
        g_variant_builder_init(&changedPropertiesBuilder,
                               G_VARIANT_TYPE_DICTIONARY);
        g_variant_builder_add(
            &changedPropertiesBuilder, "{sv}", "Position",
            g_variant_new_int64(llround(elapsedSeconds * G_USEC_PER_SEC)));

        GVariant *parameters =
            g_variant_new("(sa{sv}as)", "org.mpris.MediaPlayer2.Player",
                          &changedPropertiesBuilder, NULL);

        g_dbus_connection_emit_signal(connection, NULL,
                                      "/org/mpris/MediaPlayer2",
                                      "org.freedesktop.DBus.Properties",
                                      "PropertiesChanged", parameters, NULL);
#else
        (void)elapsedSeconds;
#endif
}

void emitSeekedSignal(double newPositionSeconds)
{
#ifndef __APPLE__
        if (newPositionSeconds < 0.0)
                newPositionSeconds = 0.0;

        const double maxSeconds = (double)(LLONG_MAX / G_USEC_PER_SEC);
        if (newPositionSeconds > maxSeconds)
                newPositionSeconds = maxSeconds;

        gint64 newPositionMicroseconds =
            llround(newPositionSeconds * G_USEC_PER_SEC);

        GVariant *parameters = g_variant_new("(x)", newPositionMicroseconds);

        g_dbus_connection_emit_signal(
            connection, NULL, "/org/mpris/MediaPlayer2",
            "org.mpris.MediaPlayer2.Player", "Seeked", parameters, NULL);
#else
        (void)newPositionSeconds;
#endif
}

void emitStringPropertyChanged(const gchar *propertyName, const gchar *newValue)
{
#ifndef __APPLE__
        GVariantBuilder changed_properties_builder;
        g_variant_builder_init(&changed_properties_builder, G_VARIANT_TYPE("a{sv}"));

        GVariant *value_variant = g_variant_new_string(newValue);
        g_variant_builder_add(&changed_properties_builder, "{sv}", propertyName, value_variant);

        GVariant *signal_variant =
            g_variant_new("(sa{sv}as)", "org.mpris.MediaPlayer2.Player", &changed_properties_builder, NULL);

        g_variant_ref_sink(signal_variant);

        g_dbus_connection_emit_signal(
            connection, NULL, "/org/mpris/MediaPlayer2",
            "org.freedesktop.DBus.Properties", "PropertiesChanged",
            signal_variant, NULL);

        g_variant_builder_clear(&changed_properties_builder);
        g_variant_unref(signal_variant);
#else
        (void)propertyName;
        (void)newValue;
#endif
}

void emitBooleanPropertyChanged(const gchar *propertyName, gboolean newValue)
{
#ifndef __APPLE__
        GVariantBuilder changed_properties_builder;
        g_variant_builder_init(&changed_properties_builder,
                               G_VARIANT_TYPE("a{sv}"));

        GVariant *value_variant = g_variant_new_boolean(newValue);
        if (value_variant == NULL)
        {
                fprintf(stderr,
                        "Failed to allocate GVariant for boolean property\n");
                return;
        }

        g_variant_builder_add(&changed_properties_builder, "{sv}", propertyName,
                              value_variant);

        GVariant *signal_variant =
            g_variant_new("(sa{sv}as)", "org.mpris.MediaPlayer2.Player",
                          &changed_properties_builder, NULL);
        if (signal_variant == NULL)
        {
                fprintf(stderr, "Failed to allocate GVariant for "
                                "PropertiesChanged signal\n");
                g_variant_builder_clear(&changed_properties_builder);
                return;
        }

        g_dbus_connection_emit_signal(
            connection, NULL, "/org/mpris/MediaPlayer2",
            "org.freedesktop.DBus.Properties", "PropertiesChanged",
            signal_variant, NULL);

        g_variant_builder_clear(&changed_properties_builder);
#else
        (void)propertyName;
        (void)newValue;
#endif
}

void playbackPause(AppState *state, struct timespec *pause_time)
{
        if (!isPaused())
        {
                emitStringPropertyChanged("PlaybackStatus", "Paused");
                clock_gettime(CLOCK_MONOTONIC, pause_time);
        }
        pausePlayback(state);
}

bool isValidAudioNode(Node *node)
{
        if (!node)
                return false;
        if (node->id < 0)
                return false;
        if (!node->song.filePath ||
            strnlen(node->song.filePath, MAXPATHLEN) == 0)
                return false;

        return true;
}

void play(Node *node, AppState *state)
{
        if (!isValidAudioNode(node))
        {
                fprintf(stderr, "Song is invalid.\n");
                return;
        }

        setCurrentSong(node);

        skipping = true;
        skipOutOfOrder = true;
        state->uiState.loadedNextSong = false;
        songLoading = true;
        forceSkip = false;

        AudioData *audioData = getAudioData();

        // Cancel starting from top
        if (state->uiState.waitingForPlaylist || audioData->restart)
        {
                state->uiState.waitingForPlaylist = false;
                audioData->restart = false;

                if (isShuffleEnabled())
                        reshufflePlaylist();
        }

        loadingdata.state = state;
        loadingdata.loadA = !usingSongDataA;
        loadingdata.loadingFirstDecoder = true;

        loadSong(node, &loadingdata, &(state->uiState));

        int maxNumTries = 50;
        int numtries = 0;

        while (!state->uiState.loadedNextSong && numtries < maxNumTries)
        {
                c_sleep(100);
                numtries++;
        }

        if (songHasErrors)
        {
                songHasErrors = false;
                forceSkip = true;

                if (node->next != NULL)
                {
                        skipToSong(node->next->id, true, state);
                        return;
                }
        }

        resetClock();
        skip(state);
}

void skipToSong(int id, bool startPlaying, AppState *state)
{
        if (songLoading || !state->uiState.loadedNextSong || skipping || clearingErrors)
                if (!forceSkip)
                        return;

        PlayList *playlist = getPlaylist();
        Node *found = NULL;

        findNodeInList(playlist, id, &found);

        if (startPlaying)
        {
                //FIXME: These code blocks are sinful
                double totalPauseSeconds = getTotalPauseSeconds();
                double pauseSeconds = getTotalPauseSeconds();

                playbackPlay(state);

                setTotalPauseSeconds(totalPauseSeconds);
                setPauseSeconds(pauseSeconds);
        }

        play(found, state);
}

void skipToBegginningOfSong(void)
{
        resetClock();

        if (getCurrentSong() != NULL)
        {
                seekPercentage(0);
                emitSeekedSignal(0.0);
        }
}

void prepareIfSkippedSilent(void)
{
        if (hasSilentlySwitched)
        {
                skipping = true;
                hasSilentlySwitched = false;
                resetClock();
                setCurrentImplementationType(NONE);
                setRepeatEnabled(false);

                AudioData *audioData = getAudioData();

                audioData->endOfListReached = false;

                usingSongDataA = !usingSongDataA;

                skipping = false;
        }
}

void playbackPlay(AppState *state)
{
        if (isPaused())
        {
                setTotalPauseSeconds(getTotalPauseSeconds() + getPauseSeconds());
                emitStringPropertyChanged("PlaybackStatus", "Playing");
        }
        else if (isStopped())
        {
                emitStringPropertyChanged("PlaybackStatus", "Playing");
        }

        if (isStopped() && !hasSilentlySwitched)
        {
                skipToBegginningOfSong();
        }

        resumePlayback(state);

        if (hasSilentlySwitched)
        {
                setTotalPauseSeconds(0);
                prepareIfSkippedSilent();
        }
}

void togglePause(AppState *state)
{
        togglePausePlayback(state);
        if (isPaused())
        {
                emitStringPropertyChanged("PlaybackStatus", "Paused");
                clock_gettime(CLOCK_MONOTONIC, &pauseTime);
        }
        else
        {
                if (hasSilentlySwitched && !skipping)
                {
                        setTotalPauseSeconds(0);
                        prepareIfSkippedSilent();
                }
                else
                {
                        setTotalPauseSeconds(getTotalPauseSeconds() + getPauseSeconds());
                }
                emitStringPropertyChanged("PlaybackStatus", "Playing");
        }
}

void toggleRepeat(AppState *state)
{

        bool repeatEnabled = isRepeatEnabled();
        bool repeatListEnabled = isRepeatListEnabled();

        if (repeatEnabled)
        {
                setRepeatEnabled(false);
                setRepeatListEnabled(true);
                emitStringPropertyChanged("LoopStatus", "List");
                state->uiSettings.repeatState = 2;
        }
        else if (repeatListEnabled)
        {
                setRepeatEnabled(false);
                setRepeatListEnabled(false);
                emitStringPropertyChanged("LoopStatus", "None");
                state->uiSettings.repeatState = 0;
        }
        else
        {
                setRepeatEnabled(true);
                setRepeatListEnabled(false);
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
        state->uiSettings.shuffleEnabled = !isShuffleEnabled();
        setShuffleEnabled(state->uiSettings.shuffleEnabled);

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

void toggleVisualizer(AppSettings *settings, UISettings *ui)
{
        ui->visualizerEnabled = !ui->visualizerEnabled;
        c_strcpy(settings->visualizerEnabled, ui->visualizerEnabled ? "1" : "0",
                 sizeof(settings->visualizerEnabled));
        restoreCursorPosition();
        triggerRefresh();
}

void quit(void) { exit(0); }

bool isCurrentSongDeleted(void)
{
        AudioData *audioData = getAudioData();

        return (audioData == NULL || audioData->currentFileIndex == 0)
                   ? getUserData()->songdataADeleted == true
                   : getUserData()->songdataBDeleted == true;
}

bool isValidSong(SongData *songData)
{
        return songData != NULL && songData->hasErrors == false &&
               songData->metadata != NULL;
}

SongData *getCurrentSongData(void)
{
        if (getCurrentSong() == NULL)
                return NULL;

        if (isCurrentSongDeleted())
                return NULL;

        SongData *songData = NULL;

        bool isDeleted = determineCurrentSongData(&songData);

        if (isDeleted)
                return NULL;

        if (!isValidSong(songData))
                return NULL;

        return songData;
}

double getCurrentSongDuration(void)
{
        double duration = 0.0;
        SongData *currentSongData = getCurrentSongData();

        if (currentSongData != NULL)
                duration = currentSongData->duration;

        return duration;
}

void calcElapsedTime(void)
{
        if (isStopped())
                return;

        clock_gettime(CLOCK_MONOTONIC, &current_time);

        double timeSinceLastUpdate =
            (double)(current_time.tv_sec - lastUpdateTime.tv_sec) +
            (double)(current_time.tv_nsec - lastUpdateTime.tv_nsec) / 1e9;

        if (!isPaused())
        {
                elapsedSeconds =
                    (double)(current_time.tv_sec - startTime.tv_sec) +
                    (double)(current_time.tv_nsec - startTime.tv_nsec) / 1e9;
                double seekElapsed = getSeekElapsed();
                double diff =
                    elapsedSeconds +
                    (seekElapsed + seekAccumulatedSeconds - getTotalPauseSeconds());
                double duration = getCurrentSongDuration();

                if (diff < 0)
                        seekElapsed -= diff;

                elapsedSeconds +=
                    seekElapsed + seekAccumulatedSeconds - getTotalPauseSeconds();

                if (elapsedSeconds > duration)
                        elapsedSeconds = duration;

                setSeekElapsed(seekElapsed);

                if (elapsedSeconds < 0.0)
                {
                        elapsedSeconds = 0.0;
                }

                if (getCurrentSong() != NULL && timeSinceLastUpdate >= 1.0)
                {
                        lastUpdateTime = current_time;
                }
        }
        else
        {
                setPauseSeconds((double)(current_time.tv_sec - pauseTime.tv_sec) +
                                (double)(current_time.tv_nsec - pauseTime.tv_nsec) / 1e9);
        }
}

void flushSeek(void)
{
        if (seekAccumulatedSeconds != 0.0)
        {
                Node *current = getCurrentSong();

                if (current != NULL)
                {
#ifdef USE_FAAD
                        if (pathEndsWith(current->song.filePath, "aac"))
                        {
                                m4a_decoder *decoder = getCurrentM4aDecoder();
                                if (decoder->fileType == k_rawAAC)
                                        return;
                        }
#endif
                }

                setSeekElapsed(getSeekElapsed() + seekAccumulatedSeconds);
                seekAccumulatedSeconds = 0.0;
                calcElapsedTime();
                double duration = getCurrentSongDuration();
                float percentage = elapsedSeconds / (float)duration * 100.0;

                if (percentage < 0.0)
                {
                        setSeekElapsed(0.0);
                        percentage = 0.0;
                }

                seekPercentage(percentage);

                emitSeekedSignal(elapsedSeconds);
        }
}

bool setPosition(gint64 newPosition)
{
        if (isPaused())
                return false;

        gint64 currentPositionMicroseconds =
            llround(elapsedSeconds * G_USEC_PER_SEC);
        double duration = getCurrentSongDuration();

        if (duration != 0.0)
        {
                gint64 step = newPosition - currentPositionMicroseconds;
                step = step / G_USEC_PER_SEC;
                seekAccumulatedSeconds += step;
                return true;
        }
        else
        {
                return false;
        }
}

bool seekPosition(gint64 offset)
{
        if (isPaused())
                return false;

        double duration = getCurrentSongDuration();
        if (duration != 0.0)
        {
                gint64 step = offset;
                step = step / G_USEC_PER_SEC;
                seekAccumulatedSeconds += step;
                return true;
        }
        else
        {
                return false;
        }
}

void seekForward(UIState *uis)
{
        Node *current = getCurrentSong();

        if (current != NULL)
        {
#ifdef USE_FAAD
                if (pathEndsWith(current->song.filePath, "aac"))
                {
                        m4a_decoder *decoder = getCurrentM4aDecoder();
                        if (decoder != NULL && decoder->fileType == k_rawAAC)
                                return;
                }
#endif
                if (isPaused())
                        return;

                double duration = current->song.duration;
                if (duration != 0.0)
                {
                        float step = 100 / uis->numProgressBars;
                        seekAccumulatedSeconds += step * duration / 100.0;
                }
                uis->isFastForwarding = true;
        }
}

void seekBack(UIState *uis)
{
        Node *current = getCurrentSong();

        if (current != NULL)
        {
#ifdef USE_FAAD
                if (pathEndsWith(current->song.filePath, "aac"))
                {

                        m4a_decoder *decoder = getCurrentM4aDecoder();
                        if (decoder != NULL && decoder->fileType == k_rawAAC)
                                return;
                }
#endif
                if (isPaused())
                        return;

                double duration = current->song.duration;
                if (duration != 0.0)
                {
                        float step = 100 / uis->numProgressBars;
                        seekAccumulatedSeconds -= step * duration / 100.0;
                }
                uis->isRewinding = true;
        }
}

Node *findSelectedEntryById(PlayList *playlist, int id)
{
        Node *node = playlist->head;

        if (node == NULL || id < 0)
                return NULL;

        bool found = false;

        for (int i = 0; i < playlist->count; i++)
        {
                if (node != NULL && node->id == id)
                {
                        found = true;
                        break;
                }
                else if (node == NULL)
                {
                        return NULL;
                }
                node = node->next;
        }

        if (found)
        {
                return node;
        }

        return NULL;
}

Node *findSelectedEntry(PlayList *playlist, int row)
{
        Node *node = playlist->head;

        if (node == NULL)
                return NULL;

        bool found = false;

        for (int i = 0; i < playlist->count; i++)
        {
                if (i == row)
                {
                        found = true;
                        break;
                }
                node = node->next;
        }

        if (found)
        {
                return node;
        }

        return NULL;
}

bool markAsEnqueued(FileSystemEntry *root, char *path)
{
        if (root == NULL)
                return false;

        if (path == NULL)
                return false;

        if (!root->isDirectory)
        {
                if (strcmp(root->fullPath, path) == 0)
                {
                        root->isEnqueued = true;
                        return true;
                }
        }
        else
        {
                FileSystemEntry *child = root->children;
                bool found = false;
                while (child != NULL)
                {
                        found = markAsEnqueued(child, path);
                        child = child->next;

                        if (found)
                                break;
                }

                if (found)
                {
                        root->isEnqueued = true;

                        return true;
                }
        }

        return false;
}

void markListAsEnqueued(FileSystemEntry *root, PlayList *playlist)
{
        Node *node = playlist->head;

        for (int i = 0; i < playlist->count; i++)
        {
                if (node == NULL)
                        break;

                if (node->song.filePath == NULL)
                        break;

                markAsEnqueued(root, node->song.filePath);

                node = node->next;
        }

        root->isEnqueued = false; // Don't mark the absolute root
}

bool markAsDequeued(FileSystemEntry *root, char *path)
{
        int numChildrenEnqueued = 0;

        if (root == NULL)
                return false;

        if (!root->isDirectory)
        {
                if (strcmp(root->fullPath, path) == 0)
                {
                        root->isEnqueued = false;
                        return true;
                }
        }
        else
        {
                FileSystemEntry *child = root->children;
                bool found = false;
                while (child != NULL)
                {
                        found = markAsDequeued(child, path);
                        child = child->next;

                        if (found)
                                break;
                }

                if (found)
                {
                        child = root->children;

                        while (child != NULL)
                        {
                                if (child->isEnqueued)
                                        numChildrenEnqueued++;

                                child = child->next;
                        }

                        if (numChildrenEnqueued == 0)
                                root->isEnqueued = false;

                        return true;
                }
        }

        return false;
}

Node *chooseNextSong(void)
{
        Node *current = getCurrentSong();

        Node *nextSong = getNextSong();

        if (nextSong != NULL)
                return nextSong;
        else if (current != NULL && current->next != NULL)
        {
                return current->next;
        }
        else
        {
                return NULL;
        }
}

void enqueueSong(FileSystemEntry *child)
{
        int id = incrementNodeId();

        PlayList *unshuffledPlaylist = getUnshuffledPlaylist();
        PlayList *playlist = getPlaylist();

        Node *node = NULL;
        createNode(&node, child->fullPath, id);
        if (addToList(unshuffledPlaylist, node) == -1)
                destroyNode(node);

        Node *node2 = NULL;
        createNode(&node2, child->fullPath, id);
        if (addToList(playlist, node2) == -1)
                destroyNode(node2);

        child->isEnqueued = 1;
        child->parent->isEnqueued = 1;
}

void silentSwitchToNext(bool loadSong, AppState *state)
{
        skipping = true;

        setNextSong(NULL);
        setCurrentSongToNext();

        AudioData *audioData = getAudioData();

        activateSwitch(audioData);

        skipOutOfOrder = true;
        usingSongDataA = (audioData != NULL && audioData->currentFileIndex == 0);

        if (loadSong)
        {
                loadNextSong(state);
                finishLoading(&(state->uiState));
                state->uiState.loadedNextSong = true;
                state->uiState.doNotifyMPRISSwitched = true;
        }

        resetClock();

        triggerRefresh();

        skipping = false;
        hasSilentlySwitched = true;
        nextSongNeedsRebuilding = true;

        setNextSong(NULL);
}

void removeCurrentlyPlayingSong(AppState *state)
{
        Node *current = getCurrentSong();
        UIState *uis = &(state->uiState);
        AudioData *audioData = getAudioData();

        if (current != NULL)
        {
                stopPlayback(state);
                emitStringPropertyChanged("PlaybackStatus", "Stopped");
                clearCurrentTrack();
                clearCurrentSong();
        }

        uis->loadedNextSong = false;
        audioData->restart = true;
        audioData->endOfListReached = true;

        if (current != NULL)
        {
                lastPlayedId = current->id;
                setSongToStartFrom(getListNext(current));
        }
        uis->waitingForNext = true;
        current = NULL;
}

void tryLoadNext(AppState *state)
{
        songHasErrors = false;
        clearingErrors = true;

        UIState *uis = &(state->uiState);

        Node *current = getCurrentSong();

        Node *tryNextSong = getTryNextSong();

        if (tryNextSong == NULL && current != NULL)
                tryNextSong = current->next;
        else if (tryNextSong != NULL)
                tryNextSong = tryNextSong->next;

        if (tryNextSong != NULL)
        {
                songLoading = true;
                loadingdata.state = state;
                loadingdata.loadA = !isUsingSongDataA();
                loadingdata.loadingFirstDecoder = false;
                loadSong(tryNextSong, &loadingdata, uis);
        }
        else
        {
                clearingErrors = false;
        }
}

void moveSongUp(AppState *state)
{
        UIState *uis = &(state->uiState);
        PlayList *unshuffledPlaylist = getUnshuffledPlaylist();
        PlayList *playlist = getPlaylist();

        if (state->currentView != PLAYLIST_VIEW)
        {
                return;
        }

        bool rebuild = false;

        int chosenRow = getChosenRow();

        Node *node = findSelectedEntry(unshuffledPlaylist, chosenRow);

        if (node == NULL)
        {
                return;
        }

        int id = node->id;

        pthread_mutex_lock(&(playlist->mutex));

        Node *current = getCurrentSong();

        if (node != NULL && current != NULL)
        {
                // Rebuild if current song, the next song or the song after are
                // affected
                if (current != NULL)
                {
                        Node *tmp = current;

                        for (int i = 0; i < 3; i++)
                        {
                                if (tmp == NULL)
                                        break;

                                if (tmp->id == id)
                                {
                                        rebuild = true;
                                }
                                tmp = tmp->next;
                        }
                }
        }

        moveUpList(unshuffledPlaylist, node);
        Node *plNode = findSelectedEntryById(playlist, node->id);

        if (!isShuffleEnabled())
                moveUpList(playlist, plNode);

        chosenRow--;
        chosenRow = (chosenRow > 0) ? chosenRow : 0;
        setChosenRow(chosenRow);

        if (rebuild && current != NULL)
        {
                node = NULL;
                nextSongNeedsRebuilding = false;

                setTryNextSong(current->next);
                setNextSong(getListNext(current));
                rebuildNextSong(getNextSong(), state);

                uis->loadedNextSong = true;
        }

        pthread_mutex_unlock(&(playlist->mutex));

        triggerRefresh();
}

void moveSongDown(AppState *state)
{
        UIState *uis = &(state->uiState);
        PlayList *unshuffledPlaylist = getUnshuffledPlaylist();
        PlayList *playlist = getPlaylist();

        if (state->currentView != PLAYLIST_VIEW)
        {
                return;
        }

        bool rebuild = false;

        int chosenRow = getChosenRow();

        Node *node = findSelectedEntry(unshuffledPlaylist, chosenRow);

        Node *current = getCurrentSong();

        if (node == NULL)
        {
                return;
        }

        int id = node->id;

        pthread_mutex_lock(&(playlist->mutex));

        if (node != NULL && current != NULL)
        {
                // Rebuild if current song, the next song or the previous song
                // are affected
                if (current != NULL)
                {
                        Node *tmp = current;

                        for (int i = 0; i < 2; i++)
                        {
                                if (tmp == NULL)
                                        break;

                                if (tmp->id == id)
                                {
                                        rebuild = true;
                                }
                                tmp = tmp->next;
                        }

                        if (current->prev != NULL && current->prev->id == id)
                                rebuild = true;
                }
        }

        moveDownList(unshuffledPlaylist, node);
        Node *plNode = findSelectedEntryById(playlist, node->id);

        if (!isShuffleEnabled())
                moveDownList(playlist, plNode);

        chosenRow++;
        chosenRow = (chosenRow >= unshuffledPlaylist->count)
                        ? unshuffledPlaylist->count - 1
                        : chosenRow;
        setChosenRow(chosenRow);

        if (rebuild && current != NULL)
        {
                node = NULL;
                nextSongNeedsRebuilding = false;

                setTryNextSong(current->next);
                setNextSong(getListNext(current));
                rebuildNextSong(getNextSong(), state);
                uis->loadedNextSong = true;
        }

        pthread_mutex_unlock(&(playlist->mutex));

        triggerRefresh();
}

void dequeueSong(FileSystemEntry *child, AppState *state)
{
        PlayList *unshuffledPlaylist = getUnshuffledPlaylist();
        PlayList *playlist = getPlaylist();

        Node *node1 =
            findLastPathInPlaylist(child->fullPath, unshuffledPlaylist);

        Node *current = getCurrentSong();

        if (node1 == NULL)
                return;

        if (current != NULL && current->id == node1->id)
        {
                removeCurrentlyPlayingSong(state);
        }
        else
        {
                if (getSongToStartFrom() != NULL)
                {
                        setSongToStartFrom(getListNext(node1));
                }
        }

        int id = node1->id;

        Node *node2 = findSelectedEntryById(playlist, id);

        if (node1 != NULL)
                deleteFromList(unshuffledPlaylist, node1);

        if (node2 != NULL)
                deleteFromList(playlist, node2);

        child->isEnqueued = 0;

        // Check if parent needs to be dequeued as well
        bool isEnqueued = false;

        FileSystemEntry *ch = child->parent->children;

        while (ch != NULL)
        {
                if (ch->isEnqueued)
                {
                        isEnqueued = true;
                        break;
                }
                ch = ch->next;
        }

        if (!isEnqueued)
        {
                child->parent->isEnqueued = 0;
                if (child->parent->parent != NULL)
                        child->parent->parent->isEnqueued = 0;
        }
}

void dequeueChildren(FileSystemEntry *parent, AppState *state)
{
        FileSystemEntry *child = parent->children;

        while (child != NULL)
        {
                if (child->isDirectory && child->children != NULL)
                {
                        dequeueChildren(child, state);
                }
                else
                {
                        dequeueSong(child, state);
                }

                child = child->next;
        }
}

void enqueueChildren(FileSystemEntry *child,
                     FileSystemEntry **firstEnqueuedEntry)
{
        while (child != NULL)
        {
                if (child->isDirectory && child->children != NULL)
                {
                        child->isEnqueued = 1;
                        enqueueChildren(child->children, firstEnqueuedEntry);
                }
                else if (!child->isEnqueued)
                {
                        if (*firstEnqueuedEntry == NULL)
                                *firstEnqueuedEntry = child;
                        enqueueSong(child);
                }

                child = child->next;
        }
}

bool hasSongChildren(FileSystemEntry *entry)
{
        FileSystemEntry *child = entry->children;
        int numSongs = 0;

        while (child != NULL)
        {
                if (!child->isDirectory)
                        numSongs++;

                child = child->next;
        }

        if (numSongs == 0)
        {
                return false;
        }

        return true;
}

bool hasDequeuedChildren(FileSystemEntry *parent)
{
        FileSystemEntry *child = parent->children;

        bool isDequeued = false;
        while (child != NULL)
        {
                if (!child->isEnqueued)
                {
                        isDequeued = true;
                }
                child = child->next;
        }

        return isDequeued;
}

bool isContainedWithin(FileSystemEntry *entry, FileSystemEntry *containingEntry)
{
        if (entry == NULL || containingEntry == NULL)
                return false;

        FileSystemEntry *tmp = entry->parent;

        while (tmp != NULL)
        {
                if (strcmp(tmp->fullPath, containingEntry->fullPath) == 0)
                        return true;

                tmp = tmp->parent;
        }

        return false;
}

void autostartIfStopped(FileSystemEntry *firstEnqueuedEntry, UIState *uis)
{
        PlayList *playlist = getPlaylist();
        AudioData *audioData = getAudioData();

        uis->waitingForPlaylist = false;
        uis->waitingForNext = true;
        audioData->endOfListReached = false;
        if (firstEnqueuedEntry != NULL)
                setSongToStartFrom(findPathInPlaylist(firstEnqueuedEntry->fullPath, playlist));
        lastPlayedId = -1;
}

FileSystemEntry *enqueueSongs(FileSystemEntry *entry, AppState *state)
{
        FileSystemEntry *chosenDir = getChosenDir();
        bool hasEnqueued = false;
        bool shuffle = false;
        FileSystemEntry *firstEnqueuedEntry = NULL;
        UIState *uis = &(state->uiState);

        if (entry != NULL)
        {
                if (entry->isDirectory)
                {
                        if (!hasSongChildren(entry) || entry->parent == NULL ||
                            (chosenDir != NULL &&
                             strcmp(entry->fullPath, chosenDir->fullPath) == 0))
                        {
                                if (hasDequeuedChildren(entry))
                                {
                                        if (entry->parent ==
                                            NULL) // Shuffle playlist if it's
                                                  // the root
                                                shuffle = true;

                                        entry->isEnqueued = 1;
                                        entry = entry->children;

                                        enqueueChildren(entry,
                                                        &firstEnqueuedEntry);

                                        nextSongNeedsRebuilding = true;

                                        hasEnqueued = true;
                                }
                                else
                                {
                                        dequeueChildren(entry, state);

                                        entry->isEnqueued = 0;

                                        nextSongNeedsRebuilding = true;
                                }
                        }
                        if (chosenDir != NULL && entry->parent != NULL &&
                            isContainedWithin(entry, chosenDir) &&
                            uis->allowChooseSongs == true)
                        {
                                // If the chosen directory is the same as the
                                // entry's parent and it is open
                                uis->openedSubDir = true;

                                FileSystemEntry *tmpc = chosenDir->children;

                                uis->numSongsAboveSubDir = 0;

                                while (tmpc != NULL)
                                {
                                        if (strcmp(entry->fullPath,
                                                   tmpc->fullPath) == 0 ||
                                            isContainedWithin(entry, tmpc))
                                                break;
                                        tmpc = tmpc->next;
                                        uis->numSongsAboveSubDir++;
                                }
                        }
                        setCurrentAsChosenDir();
                        if (uis->allowChooseSongs == true)
                        {
                                uis->collapseView = true;
                                triggerRefresh();
                        }
                        uis->allowChooseSongs = true;
                }
                else
                {
                        if (!entry->isEnqueued)
                        {
                                setNextSong(NULL);
                                nextSongNeedsRebuilding = true;
                                firstEnqueuedEntry = entry;

                                enqueueSong(entry);

                                hasEnqueued = true;
                        }
                        else
                        {
                                setNextSong(NULL);
                                nextSongNeedsRebuilding = true;

                                dequeueSong(entry, state);
                        }
                }
                triggerRefresh();
        }

        if (hasEnqueued)
        {
                autostartIfStopped(firstEnqueuedEntry, uis);
        }

        if (shuffle)
        {
                PlayList *playlist = getPlaylist();

                shufflePlaylist(playlist);
                setSongToStartFrom(NULL);
        }

        if (nextSongNeedsRebuilding)
        {
                reshufflePlaylist();
        }

        return firstEnqueuedEntry;
}

void handleRemove(AppState *state)
{
        UIState *uis = &(state->uiState);
        PlayList *playlist = getPlaylist();
        PlayList *unshuffledPlaylist = getUnshuffledPlaylist();

        if (state->currentView == PLAYLIST_VIEW)
        {

                bool rebuild = false;

                Node *node =
                    findSelectedEntry(unshuffledPlaylist, getChosenRow());

                if (node == NULL)
                {
                        return;
                }

                Node *current = getCurrentSong();
                Node *song = chooseNextSong();
                int id = node->id;
                int currentId = (current != NULL) ? current->id : -1;

                if (currentId == node->id)
                {
                        removeCurrentlyPlayingSong(state);
                }
                else
                {
                        if (getSongToStartFrom() != NULL)
                        {
                                setSongToStartFrom(getListNext(node));
                        }
                }

                pthread_mutex_lock(&(playlist->mutex));

                if (node != NULL && song != NULL && current != NULL)
                {
                        if (strcmp(song->song.filePath, node->song.filePath) ==
                                0 ||
                            (current != NULL && current->next != NULL &&
                             id == current->next->id))
                                rebuild = true;
                }

                if (node != NULL)
                        markAsDequeued(getLibrary(), node->song.filePath);

                Node *node2 = findSelectedEntryById(playlist, id);

                if (node != NULL)
                        deleteFromList(unshuffledPlaylist, node);

                if (node2 != NULL)
                        deleteFromList(playlist, node2);

                if (isShuffleEnabled())
                        rebuild = true;

                current = findSelectedEntryById(playlist, currentId);

                if (rebuild && current != NULL)
                {
                        node = NULL;
                        setNextSong(NULL);
                        reshufflePlaylist();

                        setTryNextSong(current->next);
                        nextSongNeedsRebuilding = false;
                        setNextSong(NULL);
                        setNextSong(getListNext(current));
                        rebuildNextSong(getNextSong(), state);
                        uis->loadedNextSong = true;
                }

                pthread_mutex_unlock(&(playlist->mutex));
        }
        else
        {
                return;
        }

        triggerRefresh();
}

Node *getSongByNumber(PlayList *playlist, int songNumber)
{
        Node *song = playlist->head;

        if (!song)
                return getCurrentSong();

        if (songNumber <= 0)
        {
                return song;
        }

        int count = 1;

        while (song->next != NULL && count != songNumber)
        {
                song = getListNext(song);
                count++;
        }

        return song;
}

void addToFavoritesPlaylist(void)
{
        Node *current = getCurrentSong();
        PlayList *favoritesPlaylist = getFavoritesPlaylist();

        if (current == NULL)
                return;

        int id = current->id;

        Node *node = NULL;

        if (findSelectedEntryById(favoritesPlaylist, id) !=
            NULL) // Song is already in list
                return;

        createNode(&node, current->song.filePath, id);
        addToList(favoritesPlaylist, node);
}

int loadDecoder(SongData *songData, bool *songDataDeleted)
{
        int result = 0;
        if (songData != NULL)
        {
                *songDataDeleted = false;

                // This should only be done for the second song, as
                // switchAudioImplementation() handles the first one
                if (!loadingdata.loadingFirstDecoder)
                {
                        if (hasBuiltinDecoder(songData->filePath))
                                result = prepareNextDecoder(songData->filePath);
                        else if (pathEndsWith(songData->filePath, "opus"))
                                result =
                                    prepareNextOpusDecoder(songData->filePath);
                        else if (pathEndsWith(songData->filePath, "ogg"))
                                result = prepareNextVorbisDecoder(
                                    songData->filePath);
                        else if (pathEndsWith(songData->filePath, "webm"))
                                result = prepareNextWebmDecoder(songData);
#ifdef USE_FAAD
                        else if (pathEndsWith(songData->filePath, "m4a") ||
                                 pathEndsWith(songData->filePath, "aac"))
                                result = prepareNextM4aDecoder(songData);

#endif
                }
        }
        return result;
}

int assignLoadedData(void)
{
        int result = 0;

        if (loadingdata.loadA)
        {
                getUserData()->songdataA = loadingdata.songdataA;
                result = loadDecoder(loadingdata.songdataA,
                                     &(getUserData()->songdataADeleted));
        }
        else
        {
                getUserData()->songdataB = loadingdata.songdataB;
                result = loadDecoder(loadingdata.songdataB,
                                     &(getUserData()->songdataBDeleted));
        }

        return result;
}

void *songDataReaderThread(void *arg)
{
        LoadingThreadData *loadingdata = (LoadingThreadData *)arg;

        // Acquire the mutex lock
        pthread_mutex_lock(&(loadingdata->mutex));

        char filepath[MAXPATHLEN];
        c_strcpy(filepath, loadingdata->filePath, sizeof(filepath));

        SongData *songdata = NULL;

        if (loadingdata->loadA)
        {
                if (!getUserData()->songdataADeleted)
                {
                        getUserData()->songdataADeleted = true;
                        unloadSongData(&(loadingdata->songdataA), loadingdata->state);
                }
        }
        else
        {
                if (!getUserData()->songdataBDeleted)
                {
                        getUserData()->songdataBDeleted = true;
                        unloadSongData(&(loadingdata->songdataB), loadingdata->state);
                }
        }

        if (existsFile(filepath) >= 0)
        {
                songdata = loadSongData(filepath, loadingdata->state);
        }
        else
                songdata = NULL;

        if (loadingdata->loadA)
        {
                loadingdata->songdataA = songdata;
        }
        else
        {
                loadingdata->songdataB = songdata;
        }

        int result = assignLoadedData();

        if (result < 0)
                songdata->hasErrors = true;

        // Release the mutex lock
        pthread_mutex_unlock(&(loadingdata->mutex));

        if (songdata == NULL || songdata->hasErrors)
        {
                songHasErrors = true;
                clearingErrors = true;
                setNextSong(NULL);
        }
        else
        {
                songHasErrors = false;
                clearingErrors = false;
                setNextSong(getTryNextSong());
                setTryNextSong(NULL);
        }

        loadingdata->state->uiState.loadedNextSong = true;
        skipping = false;
        songLoading = false;

        return NULL;
}

void loadSong(Node *song, LoadingThreadData *loadingdata, UIState *uis)
{
        if (song == NULL)
        {
                uis->loadedNextSong = true;
                skipping = false;
                songLoading = false;
                return;
        }

        c_strcpy(loadingdata->filePath, song->song.filePath,
                 sizeof(loadingdata->filePath));

        pthread_t loadingThread;
        pthread_create(&loadingThread, NULL, songDataReaderThread,
                       (void *)loadingdata);
}

void rebuildNextSong(Node *song, AppState *state)
{
        if (song == NULL)
                return;

        UIState *uis = &(state->uiState);

        loadingdata.state = state;
        loadingdata.loadA = !usingSongDataA;
        loadingdata.loadingFirstDecoder = false;

        songLoading = true;

        loadSong(song, &loadingdata, uis);

        int maxNumTries = 50;
        int numtries = 0;

        while (songLoading && !uis->loadedNextSong && numtries < maxNumTries)
        {
                c_sleep(100);
                numtries++;
        }
        songLoading = false;
}

void stop(AppState *state)
{
        stopPlayback(state);

        if (isStopped())
        {
                skipToBegginningOfSong();
                emitStringPropertyChanged("PlaybackStatus", "Stopped");
        }
}

int currentSort = 0;

void sortLibrary(void)
{
        FileSystemEntry *library = getLibrary();

        if (currentSort == 0)
        {
                sortFileSystemTree(library,
                                   compareFoldersByAgeFilesAlphabetically);
                currentSort = 1;
        }
        else
        {
                sortFileSystemTree(library, compareEntryNatural);
                currentSort = 0;
        }

        triggerRefresh();
}

void loadNextSong(AppState *state)
{
        UIState *uis = &(state->uiState);

        songLoading = true;
        nextSongNeedsRebuilding = false;
        skipFromStopped = false;
        loadingdata.loadA = !usingSongDataA;
        setTryNextSong(getListNext(getCurrentSong()));
        setNextSong(getTryNextSong());
        loadingdata.state = state;
        loadingdata.loadingFirstDecoder = false;
        loadSong(getNextSong(), &loadingdata, uis);
}

bool determineCurrentSongData(SongData **currentSongData)
{
        AudioData *audioData = getAudioData();

        *currentSongData = (audioData->currentFileIndex == 0)
                               ? getUserData()->songdataA
                               : getUserData()->songdataB;

        bool isDeleted = (audioData->currentFileIndex == 0)
                             ? getUserData()->songdataADeleted == true
                             : getUserData()->songdataBDeleted == true;

        if (isDeleted)
        {
                *currentSongData = (audioData->currentFileIndex != 0)
                                       ? getUserData()->songdataA
                                       : getUserData()->songdataB;

                isDeleted = (audioData->currentFileIndex != 0)
                                ? getUserData()->songdataADeleted == true
                                : getUserData()->songdataBDeleted == true;

                if (!isDeleted)
                {
                        activateSwitch(audioData);
                        audioData->switchFiles = false;
                }
        }
        return isDeleted;
}

void setCurrentSongToNext(void)
{
        Node *current = getCurrentSong();

        if (current != NULL)
                lastPlayedId = current->id;

        setCurrentSong(chooseNextSong());
}

void finishLoading(UIState *uis)
{
        int maxNumTries = 20;
        int numtries = 0;

        while (!uis->loadedNextSong && numtries < maxNumTries)
        {
                c_sleep(100);
                numtries++;
        }

        uis->loadedNextSong = true;
}

void resetClock(void)
{
        elapsedSeconds = 0.0;
        setPauseSeconds(0.0);
        setTotalPauseSeconds(0.0);
        setSeekElapsed(0.0);
        clock_gettime(CLOCK_MONOTONIC, &startTime);
}

void repeatList(AppState *state)
{
        AudioData *audioData = getAudioData();

        state->uiState.waitingForPlaylist = true;
        nextSongNeedsRebuilding = true;
        audioData->endOfListReached = false;
}

void skipToNextSong(AppState *state)
{
        Node *current = getCurrentSong();

        // Stop if there is no song or no next song
        if (current == NULL || current->next == NULL)
        {
                if (isRepeatListEnabled())
                {
                        clearCurrentSong();
                }
                else if (!isStopped() && !isPaused())
                {
                        stop(state);
                        return;
                }
                else
                {
                        return;
                }
        }

        if (songLoading || nextSongNeedsRebuilding || skipping ||
            clearingErrors)
                return;

        if (isStopped() || isPaused())
        {
                silentSwitchToNext(true, state);
                return;
        }

        if (isShuffleEnabled())
                state->uiState.resetPlaylistDisplay = true;

        double totalPauseSeconds = getTotalPauseSeconds();
        double pauseSeconds = getTotalPauseSeconds();

        playbackPlay(state);

        setTotalPauseSeconds(totalPauseSeconds);
        setPauseSeconds(pauseSeconds);

        skipping = true;
        skipOutOfOrder = false;

        resetClock();

        skip(state);
}

void setCurrentSongToPrev(void)
{
        Node *current = getCurrentSong();

        if (current != NULL && current->prev != NULL)
        {
                lastPlayedId = current->id;
                setCurrentSong(current->prev);
        }
}

void silentSwitchToPrev(AppState *state)
{
        AudioData *audioData = getAudioData();

        skipping = true;

        setCurrentSongToPrev();
        activateSwitch(audioData);

        state->uiState.loadedNextSong = false;
        songLoading = true;
        forceSkip = false;

        usingSongDataA = !usingSongDataA;

        loadingdata.state = state;
        loadingdata.loadA = usingSongDataA;
        loadingdata.loadingFirstDecoder = true;
        loadSong(getCurrentSong(), &loadingdata, &(state->uiState));
        state->uiState.doNotifyMPRISSwitched = true;
        finishLoading(&(state->uiState));

        resetClock();

        triggerRefresh();
        skipping = false;
        nextSongNeedsRebuilding = true;
        setNextSong(NULL);

        skipOutOfOrder = true;
        hasSilentlySwitched = true;
}

void skipToPrevSong(AppState *state)
{
        Node *current = getCurrentSong();

        if (current == NULL)
        {
                if (!isStopped() && !isPaused())
                        stop(state);
                return;
        }

        if (songLoading || skipping || clearingErrors)
                if (!forceSkip)
                        return;

        if (isStopped() || isPaused())
        {
                silentSwitchToPrev(state);
                return;
        }

        Node *song = current;

        setCurrentSongToPrev();

        if (song == current)
        {
                resetClock();
                updatePlaybackPosition(
                    0); // We need to signal to mpris that the song was
                        // reset to the beginning
        }

        double totalPauseSeconds = getTotalPauseSeconds();
        double pauseSeconds = getTotalPauseSeconds();

        playbackPlay(state);

        setTotalPauseSeconds(totalPauseSeconds);
        setPauseSeconds(pauseSeconds);

        skipping = true;
        skipOutOfOrder = true;
        state->uiState.loadedNextSong = false;
        songLoading = true;
        forceSkip = false;

        loadingdata.state = state;
        loadingdata.loadA = !usingSongDataA;
        loadingdata.loadingFirstDecoder = true;
        loadSong(getCurrentSong(), &loadingdata, &(state->uiState));
        int maxNumTries = 50;
        int numtries = 0;

        while (!state->uiState.loadedNextSong && numtries < maxNumTries)
        {
                c_sleep(100);
                numtries++;
        }

        if (songHasErrors)
        {
                songHasErrors = false;
                forceSkip = true;
                skipToPrevSong(state);
        }

        resetClock();

        skip(state);
}

void skipToNumberedSong(int songNumber, AppState *state)
{
        UIState *uis = &(state->uiState);
        PlayList *unshuffledPlaylist = getUnshuffledPlaylist();
        PlayList *playlist = getPlaylist();

        if (songLoading || !uis->loadedNextSong || skipping || clearingErrors)
                if (!forceSkip)
                        return;

        double totalPauseSeconds = getTotalPauseSeconds();
        double pauseSeconds = getTotalPauseSeconds();

        playbackPlay(state);

        setTotalPauseSeconds(totalPauseSeconds);
        setPauseSeconds(pauseSeconds);

        skipping = true;
        skipOutOfOrder = true;
        uis->loadedNextSong = false;
        songLoading = true;
        forceSkip = false;

        setCurrentSong(getSongByNumber(unshuffledPlaylist, songNumber));

        loadingdata.state = state;
        loadingdata.loadA = !usingSongDataA;
        loadingdata.loadingFirstDecoder = true;
        loadSong(getCurrentSong(), &loadingdata, uis);
        int maxNumTries = 50;
        int numtries = 0;

        while (!uis->loadedNextSong && numtries < maxNumTries)
        {
                c_sleep(100);
                numtries++;
        }

        if (songHasErrors)
        {
                songHasErrors = false;
                forceSkip = true;
                if (songNumber < playlist->count)
                        skipToNumberedSong(songNumber + 1, state);
        }

        resetClock();
        skip(state);
}

void skipToLastSong(AppState *state)
{
        PlayList *playlist = getPlaylist();

        Node *song = playlist->head;

        if (!song)
                return;

        int count = 1;
        while (song->next != NULL)
        {
                song = getListNext(song);
                count++;
        }
        skipToNumberedSong(count, state);
}

void loadFirstSong(Node *song, AppState *state)
{
        if (song == NULL)
                return;

        loadingdata.state = state;
        loadingdata.loadingFirstDecoder = true;
        loadSong(song, &loadingdata, &(state->uiState));

        int i = 0;
        while (!state->uiState.loadedNextSong && i < 10000)
        {
                if (i != 0 && i % 1000 == 0 && state->uiSettings.uiEnabled)
                        printf(".");
                c_sleep(10);
                fflush(stdout);
                i++;
        }
}

void unloadSongA(AppState *state)
{
        if (getUserData()->songdataADeleted == false)
        {
                getUserData()->songdataADeleted = true;
                unloadSongData(&(loadingdata.songdataA), state);
                getUserData()->songdataA = NULL;
        }
}

void unloadSongB(AppState *state)
{
        if (getUserData()->songdataBDeleted == false)
        {
                getUserData()->songdataBDeleted = true;
                unloadSongData(&(loadingdata.songdataB), state);
                getUserData()->songdataB = NULL;
        }
}

void unloadPreviousSong(AppState *state)
{
        pthread_mutex_lock(&(loadingdata.mutex));

        UserData *userData = getUserData();
        AudioData *audioData = getAudioData();

        if (usingSongDataA &&
            (skipping || (userData->currentSongData == NULL ||
                          userData->songdataADeleted == false ||
                          (loadingdata.songdataA != NULL &&
                           userData->songdataADeleted == false &&
                           userData->currentSongData->hasErrors == 0 &&
                           userData->currentSongData->trackId != NULL &&
                           strcmp(loadingdata.songdataA->trackId,
                                  userData->currentSongData->trackId) != 0))))
        {
                unloadSongA(state);

                if (!audioData->endOfListReached)
                        state->uiState.loadedNextSong = false;

                usingSongDataA = false;
        }
        else if (!usingSongDataA &&
                 (skipping ||
                  (userData->currentSongData == NULL ||
                   userData->songdataBDeleted == false ||
                   (loadingdata.songdataB != NULL &&
                    userData->songdataBDeleted == false &&
                    userData->currentSongData->hasErrors == 0 &&
                    userData->currentSongData->trackId != NULL &&
                    strcmp(loadingdata.songdataB->trackId,
                           userData->currentSongData->trackId) != 0))))
        {
                unloadSongB(state);

                if (!audioData->endOfListReached)
                        state->uiState.loadedNextSong = false;

                usingSongDataA = true;
        }

        pthread_mutex_unlock(&(loadingdata.mutex));
}

int loadFirst(Node *song, AppState *state)
{
        loadFirstSong(song, state);
        Node *current = getCurrentSong();

        usingSongDataA = true;

        while (songHasErrors && current->next != NULL)
        {
                songHasErrors = false;
                state->uiState.loadedNextSong = false;
                current = current->next;
                loadFirstSong(current, state);
        }

        if (songHasErrors)
        {
                // Couldn't play any of the songs
                unloadPreviousSong(state);
                current = NULL;
                songHasErrors = false;
                return -1;
        }

        UserData *userData = getUserData();

        userData->currentPCMFrame = 0;
        userData->currentSongData = userData->songdataA;

        return 0;
}

typedef struct
{
        char *path;
        AppState *state;
} UpdateLibraryArgs;

void *updateLibraryThread(void *arg)
{
        if (arg == NULL)
                return NULL;

        UpdateLibraryArgs *args = arg;
        FileSystemEntry *library = getLibrary();

        char *path = args->path;
        AppState *state = args->state;
        int tmpDirectoryTreeEntries = 0;

        setErrorMessage("Updating Library...");

        FileSystemEntry *tmp =
            createDirectoryTree(path, &tmpDirectoryTreeEntries);

        if (!tmp)
        {
                perror("createDirectoryTree");
                pthread_mutex_unlock(&(state->switchMutex));
                return NULL;
        }

        pthread_mutex_lock(&(state->switchMutex));

        copyIsEnqueued(library, tmp);

        freeTree(library);
        library = tmp;
        state->uiState.numDirectoryTreeEntries = tmpDirectoryTreeEntries;
        resetChosenDir();

        pthread_mutex_unlock(&(state->switchMutex));

        c_sleep(1000); // Don't refresh immediately or we risk the error message
                       // not clearing
        triggerRefresh();

        free(args->path);
        free(args);

        return NULL;
}

void updateLibrary(AppState *state, char *path)
{
        pthread_t threadId;

        freeSearchResults();

        UpdateLibraryArgs *args = malloc(sizeof(UpdateLibraryArgs));

        if (!args)
                return; // handle allocation failure

        args->path = strdup(path);
        args->state = state;

        if (pthread_create(&threadId, NULL, updateLibraryThread, args) != 0)
        {
                perror("Failed to create thread");
                return;
        }
}

void askIfCacheLibrary(UISettings *ui)
{
        if (ui->cacheLibrary >
            -1) // Only use this function if cacheLibrary isn't set
                return;

        char input = '\0';

        restoreTerminalMode();
        enableInputBuffering();
        showCursor();

        printf("Would you like to enable a (local) library cache for quicker "
               "startup "
               "times?\nYou can update the cache at any time by pressing 'u'. "
               "(y/n): ");

        fflush(stdout);

        do
        {
                int res = scanf(" %c", &input);

                if (res < 0)
                        break;

        } while (input != 'Y' && input != 'y' && input != 'N' && input != 'n');

        if (input == 'Y' || input == 'y')
        {
                printf("Y\n");
                ui->cacheLibrary = 1;
        }
        else
        {
                printf("N\n");
                ui->cacheLibrary = 0;
        }

        setNonblockingMode();
        setRawInputMode();
        hideCursor();
}

void createLibrary(AppSettings *settings, AppState *state)
{
        FileSystemEntry *library = getLibrary();

        if (state->uiSettings.cacheLibrary > 0)
        {
                char *libFilepath = getLibraryFilePath();
                library = reconstructTreeFromFile(
                    libFilepath, settings->path,
                    &(state->uiState.numDirectoryTreeEntries));
                free(libFilepath);
                updateLibraryIfChangedDetected(state);
        }

        if (library == NULL || library->children == NULL)
        {
                struct timeval start, end;

                gettimeofday(&start, NULL);

                library = createDirectoryTree(
                    settings->path, &(state->uiState.numDirectoryTreeEntries));

                gettimeofday(&end, NULL);
                long seconds = end.tv_sec - start.tv_sec;
                long microseconds = end.tv_usec - start.tv_usec;
                double elapsed = seconds + microseconds * 1e-6;

                // If time to load the library was significant, ask to use cache
                // instead
                if (elapsed > ASK_IF_USE_CACHE_LIMIT_SECONDS)
                {
                        askIfCacheLibrary(&(state->uiSettings));
                }
        }

        if (library == NULL || library->children == NULL)
        {
                char message[MAXPATHLEN + 64];

                snprintf(message, MAXPATHLEN + 64, "No music found at %s.",
                         settings->path);

                setErrorMessage(message);
        }
}

time_t getModificationTime(struct stat *path_stat)
{

        if (path_stat->st_mtime != 0)
        {
                return path_stat->st_mtime;
        }
        else
        {
#ifdef __APPLE__
                return path_stat->st_mtimespec.tv_sec; // macOS-specific member.
#else
                return path_stat->st_mtim.tv_sec; // Linux-specific member.
#endif
        }
}

void *updateIfTopLevelFoldersMtimesChangedThread(void *arg)
{
        UpdateLibraryThreadArgs *args = (UpdateLibraryThreadArgs *)
            arg; // Cast `arg` back to the structure pointer
        char *path = args->path;
        AppState *state = args->state;
        UISettings *ui = &(state->uiSettings);

        struct stat path_stat;

        if (stat(path, &path_stat) == -1)
        {
                perror("stat");
                free(args);
                pthread_exit(NULL);
        }

        if (getModificationTime(&path_stat) > ui->lastTimeAppRan &&
            ui->lastTimeAppRan > 0)
        {
                updateLibrary(state, path);
                free(args);
                pthread_exit(NULL);
        }

        DIR *dir = opendir(path);
        if (!dir)
        {
                perror("opendir");
                free(args);
                pthread_exit(NULL);
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {

                if (strcmp(entry->d_name, ".") == 0 ||
                    strcmp(entry->d_name, "..") == 0)
                {
                        continue;
                }

                char fullPath[1024];
                snprintf(fullPath, sizeof(fullPath), "%s/%s", path,
                         entry->d_name);

                if (stat(fullPath, &path_stat) == -1)
                {
                        perror("stat");
                        continue;
                }

                if (S_ISDIR(path_stat.st_mode))
                {

                        if (getModificationTime(&path_stat) >
                                ui->lastTimeAppRan &&
                            ui->lastTimeAppRan > 0)
                        {
                                updateLibrary(state, path);
                                break;
                        }
                }
        }

        closedir(dir);

        free(args);

        pthread_exit(NULL);
}

// This only checks the library mtime and toplevel subfolders mtimes
void updateLibraryIfChangedDetected(AppState *state)
{
        pthread_t tid;

        UpdateLibraryThreadArgs *args = malloc(sizeof(UpdateLibraryThreadArgs));

        if (args == NULL)
        {
                perror("malloc");
                return;
        }

        AppSettings *settings = getAppSettings();

        args->path = settings->path;
        args->state = state;

        if (pthread_create(&tid, NULL,
                           updateIfTopLevelFoldersMtimesChangedThread,
                           (void *)args) != 0)
        {
                perror("pthread_create");
                free(args);
        }
}

// Go through the display playlist and the shuffle playlist to remove all songs
// except the current one. If no active song (if stopped rather than paused for
// example) entire playlist will be removed
void updatePlaylistToPlayingSong(AppState *state)
{

        bool clearAll = false;
        int currentID = -1;

        Node *current = getCurrentSong();
        PlayList *unshuffledPlaylist = getUnshuffledPlaylist();
        PlayList *playlist = getPlaylist();

        // Do we need to clear the entire playlist?
        if (current == NULL)
        {
                clearAll = true;
        }
        else
        {
                currentID = current->id;
        }

        int nextInPlaylistID;
        pthread_mutex_lock(&(playlist->mutex));
        Node *songToBeRemoved;
        Node *nextInPlaylist = unshuffledPlaylist->head;

        while (nextInPlaylist != NULL)
        {
                nextInPlaylistID = nextInPlaylist->id;

                if (clearAll || nextInPlaylistID != currentID)
                {
                        songToBeRemoved = nextInPlaylist;

                        nextInPlaylist = nextInPlaylist->next;

                        int id = songToBeRemoved->id;

                        // Update Library
                        if (songToBeRemoved != NULL)
                                markAsDequeued(getLibrary(),
                                               songToBeRemoved->song.filePath);

                        // Remove from Display playlist
                        if (songToBeRemoved != NULL)
                                deleteFromList(unshuffledPlaylist,
                                               songToBeRemoved);

                        // Remove from Shuffle playlist
                        Node *node2 = findSelectedEntryById(playlist, id);
                        if (node2 != NULL)
                                deleteFromList(playlist, node2);
                }
                else
                {
                        nextInPlaylist = nextInPlaylist->next;
                }
        }
        pthread_mutex_unlock(&(playlist->mutex));

        nextSongNeedsRebuilding = true;
        setNextSong(NULL);

        // Only refresh the screen if it makes sense to do so
        if (state->currentView == PLAYLIST_VIEW ||
            state->currentView == LIBRARY_VIEW)
        {
                triggerRefresh();
        }
}
