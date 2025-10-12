#include "playerops.h"
#include "appstate.h"
#include "systemintegration.h"
#include "playback.h"
#include "player_ui.h"
#include "search_ui.h"
#include "songloader.h"
#include "common.h"
#include "soundcommon.h"
#include "term.h"
#include "theme.h"
#include "utils.h"
#include <ctype.h>
#include <dirent.h>
#include "playlist_ops.h"
#include "library_ops.h"
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

#ifndef G_USEC_PER_SEC
#define G_USEC_PER_SEC 1000000
#endif

static GMainContext *globalMainContext = NULL;

void setGMainContext(GMainContext *val)
{
        globalMainContext = val;
}

void *getGMainContext(void)
{
        return globalMainContext;
}

bool shouldRefreshPlayer()
{
        PlaybackState *ps = getPlaybackState();

        return !ps->skipping && !isEOFReached() && !isImplSwitchReached();
}

void handleSkipFromStopped(void)
{
        PlaybackState *ps = getPlaybackState();

        // If we don't do this the song gets loaded in the wrong slot
        if (ps->skipFromStopped)
        {
                ps->usingSongDataA = !ps->usingSongDataA;
                ps->skipOutOfOrder = false;
                ps->skipFromStopped = false;
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

FileSystemEntry *enqueueSongs(FileSystemEntry *entry, AppState *state)
{
        FileSystemEntry *chosenDir = getChosenDir();
        bool hasEnqueued = false;
        bool shuffle = false;
        FileSystemEntry *firstEnqueuedEntry = NULL;
        UIState *uis = &(state->uiState);
        PlaybackState *ps = getPlaybackState();

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

                                        ps->nextSongNeedsRebuilding = true;

                                        hasEnqueued = true;
                                }
                                else
                                {
                                        dequeueChildren(entry, state);

                                        entry->isEnqueued = 0;

                                        ps->nextSongNeedsRebuilding = true;
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
                                ps->nextSongNeedsRebuilding = true;
                                firstEnqueuedEntry = entry;

                                enqueueSong(entry);

                                hasEnqueued = true;
                        }
                        else
                        {
                                setNextSong(NULL);
                                ps->nextSongNeedsRebuilding = true;

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

        if (ps->nextSongNeedsRebuilding)
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

                Node *node = findSelectedEntry(unshuffledPlaylist, getChosenRow());

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
                        PlaybackState *ps = getPlaybackState();
                        ps->nextSongNeedsRebuilding = false;
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

void rebuildNextSong(Node *song, AppState *state)
{
        if (song == NULL)
                return;

        UIState *uis = &(state->uiState);
        PlaybackState *ps = getPlaybackState();

        ps->loadingdata.state = state;
        ps->loadingdata.loadA = !ps->usingSongDataA;
        ps->loadingdata.loadingFirstDecoder = false;

        ps->songLoading = true;

        loadSong(song, &ps->loadingdata, uis);

        int maxNumTries = 50;
        int numtries = 0;

        while (ps->songLoading && !uis->loadedNextSong && numtries < maxNumTries)
        {
                c_sleep(100);
                numtries++;
        }
        ps->songLoading = false;
}

void loadNextSong(AppState *state)
{
        UIState *uis = &(state->uiState);
        PlaybackState *ps = getPlaybackState();

        ps->songLoading = true;
        ps->nextSongNeedsRebuilding = false;
        ps->skipFromStopped = false;
        ps->loadingdata.loadA = !ps->usingSongDataA;
        setTryNextSong(getListNext(getCurrentSong()));
        setNextSong(getTryNextSong());
        ps->loadingdata.state = state;
        ps->loadingdata.loadingFirstDecoder = false;
        loadSong(getNextSong(), &ps->loadingdata, uis);
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

