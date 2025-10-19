/* kew - Music For The Shell
Copyright (C) 2022 Ravachol

http://codeberg.org/ravachol/kew

$$\
$$ |
$$ |  $$\  $$$$$$\  $$\  $$\  $$\
$$ | $$  |$$  __$$\ $$ | $$ | $$ |
$$$$$$  / $$$$$$$$ |$$ | $$ | $$ |
$$  _$$<  $$   ____|$$ | $$ | $$ |
$$ | \$$\ \$$$$$$$\ \$$$$$\$$$$  |
\__|  \__| \_______| \_____\____/

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#ifndef __USE_POSIX
#define __USE_POSIX
#endif

#ifdef __FreeBSD__
#define __BSD_VISIBLE 1
#endif

#include "common/appstate.h"
#include "common/common.h"
#include "common/events.h"

#include "sys/mpris.h"
#include "sys/notifications.h"
#include "sys/systemintegration.h"

#include "ui/common_ui.h"
#include "ui/input.h"
#include "ui/control_ui.h"
#include "ui/player_ui.h"
#include "ui/playlist_ui.h"
#include "ui/search_ui.h"
#include "ui/visuals.h"
#include "ui/queue_ui.h"
#include "ui/cli.h"
#include "ui/settings.h"

#include "ops/library_ops.h"
#include "ops/playback_clock.h"
#include "ops/playback_ops.h"
#include "ops/playback_state.h"
#include "ops/playback_system.h"
#include "ops/playlist_ops.h"
#include "ops/trackmanager.h"

#include "utils/cache.h"
#include "utils/term.h"
#include "utils/utils.h"
#include "utils/file.h"

#include <fcntl.h>
#include <gio/gio.h>
#include <glib-unix.h>
#include <glib.h>
#include <locale.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

const char VERSION[] = "3.6.3";

AppState *statePtr = NULL;

void handleInput(void)
{
        struct Event event = processInput();

        AppState *state = getAppState();
        AppSettings *settings = getAppSettings();
        PlayList *playlist = getPlaylist();
        int chosenRow = getChosenRow();

        switch (event.type)
        {
                break;
        case EVENT_ENQUEUE:
                viewEnqueue(false);
                break;
        case EVENT_ENQUEUEANDPLAY:
                viewEnqueue(true);
                break;
        case EVENT_PLAY_PAUSE:
                togglePause();
                break;
        case EVENT_TOGGLEVISUALIZER:
                toggleVisualizer();
                break;
        case EVENT_TOGGLEREPEAT:
                toggleRepeat();
                break;
        case EVENT_TOGGLEASCII:
                toggleAscii();
                break;
        case EVENT_TOGGLENOTIFICATIONS:
                toggleNotifications();
                break;
        case EVENT_SHUFFLE:
                toggleShuffle();
                break;
        case EVENT_SHOWLYRICSPAGE:
                toggleShowLyricsPage();
                break;
        case EVENT_CYCLECOLORMODE:
                cycleColorMode();
                break;
        case EVENT_CYCLETHEMES:
                cycleThemes();
                break;
        case EVENT_QUIT:
                quit();
                break;
        case EVENT_SCROLLNEXT:
                scrollNext();
                break;
        case EVENT_SCROLLPREV:
                scrollPrev();
                break;
        case EVENT_VOLUME_UP:
                volumeChange(5);
                emitVolumeChanged();
                break;
        case EVENT_VOLUME_DOWN:
                volumeChange(-5);
                emitVolumeChanged();
                break;
        case EVENT_NEXT:
                skipToNextSong();
                break;
        case EVENT_PREV:
                skipToPrevSong();
                break;
        case EVENT_SEEKBACK:
                seekBack();
                break;
        case EVENT_SEEKFORWARD:
                seekForward();
                break;
        case EVENT_ADDTOFAVORITESPLAYLIST:
                addToFavoritesPlaylist();
                break;
        case EVENT_EXPORTPLAYLIST:
                exportCurrentPlaylist(settings->path, playlist);
                break;
        case EVENT_UPDATELIBRARY:
                freeSearchResults();
                updateLibrary(settings->path);
                break;
        case EVENT_SHOWKEYBINDINGS:
                toggleShowView(KEYBINDINGS_VIEW);
                break;
        case EVENT_SHOWPLAYLIST:
                toggleShowView(PLAYLIST_VIEW);
                break;
        case EVENT_SHOWSEARCH:
                toggleShowView(SEARCH_VIEW);
                break;
                break;
        case EVENT_SHOWLIBRARY:
                toggleShowView(LIBRARY_VIEW);
                break;
        case EVENT_NEXTPAGE:
                flipNextPage();
                break;
        case EVENT_PREVPAGE:
                flipPrevPage();
                break;
        case EVENT_REMOVE:
                handleRemove(getChosenRow());
                resetListAfterDequeuingPlayingSong();
                break;
        case EVENT_SHOWTRACK:
                showTrack();
                break;
        case EVENT_NEXTVIEW:
                switchToNextView();
                break;
        case EVENT_PREVVIEW:
                switchToPreviousView();
                break;
        case EVENT_CLEARPLAYLIST:
                dequeueAllExceptPlayingSong();
                state->uiState.resetPlaylistDisplay = true;
                break;
        case EVENT_MOVESONGUP:
                moveSongUp(&chosenRow);
                setChosenRow(chosenRow);
                break;
        case EVENT_MOVESONGDOWN:
                moveSongDown(&chosenRow);
                setChosenRow(chosenRow);
                break;
        case EVENT_STOP:
                stop();
                break;
        case EVENT_SORTLIBRARY:
                sortLibrary();
                break;

        default:
                state->uiState.isFastForwarding = false;
                state->uiState.isRewinding = false;
                break;
        }
}

void updatePlayer(void)
{
        AppState *state = getAppState();
        UIState *uis = &(state->uiState);
        struct winsize ws;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);

        // Check if window has changed size
        if (ws.ws_col != state->uiState.windowSize.ws_col || ws.ws_row != state->uiState.windowSize.ws_row)
        {
                uis->resizeFlag = 1;
                state->uiState.windowSize = ws;
        }

        // resizeFlag can also be set by handleResize
        if (uis->resizeFlag)
        {
                resize(uis);
        }
        else
        {
                refreshPlayer();
        }
}

void prepareNextSong(void)
{
        AppState *state = getAppState();

        resetClock();
        handleSkipOutOfOrder();
        finishLoading();

        setNextSong(NULL);
        triggerRefresh();

        Node *current = getCurrentSong();

        if (!opsIsRepeatEnabled() || current == NULL)
        {
                unloadPreviousSong();
        }

        if (current == NULL)
        {
                if (state->uiSettings.quitAfterStopping)
                {
                        quit();
                }
                else
                {
                        setEndOfListReached();
                }
        }
        else
        {
                determineSongAndNotify();
        }
}

int prepareAndPlaySong(Node *song)
{
        if (!song)
                return -1;

        setCurrentSong(song);

        unloadSongA();
        unloadSongB();

        int res = loadFirst(getCurrentSong());

        finishLoading();

        if (res >= 0)
        {
                res = playbackCreate();
        }

        if (res >= 0)
        {
                resumePlayback();
        }

        if (res < 0)
                setEndOfListReached();

        triggerRefresh();
        resetClock();

        return res;
}

void checkAndLoadNextSong(void)
{
        AppState *state = getAppState();
        PlaybackState *ps = getPlaybackState();

        if (audioData.restart)
        {
                PlayList *playlist = getPlaylist();
                if (playlist->head == NULL)
                        return;

                if (ps->waitingForPlaylist || ps->waitingForNext)
                {
                        ps->songLoading = true;

                        Node *nextSong = determineNextSong(playlist);
                        if (!nextSong)
                                return;

                        audioData.restart = false;
                        ps->waitingForPlaylist = false;
                        ps->waitingForNext = false;
                        state->uiState.songWasRemoved = false;

                        if (isShuffleEnabled())
                                reshufflePlaylist();

                        int res = prepareAndPlaySong(nextSong);

                        if (res < 0)
                                setEndOfListReached();

                        ps->loadedNextSong = false;
                        setNextSong(NULL);
                }
        }
        else if (getCurrentSong() != NULL &&
                 (ps->nextSongNeedsRebuilding || getNextSong() == NULL) &&
                 !ps->songLoading)
        {
                updateNextSongIfNeeded();
        }
}

void updateAndLoad(void)
{
        updatePlayer();

        PlayList *playlist = getPlaylist();
        AudioData *audioData = getAudioData();
        PlaybackState *ps = getPlaybackState();

        if (playlist->head != NULL)
        {
                if ((ps->skipFromStopped || !ps->loadedNextSong ||
                     ps->nextSongNeedsRebuilding) &&
                    !audioData->endOfListReached)
                {
                        checkAndLoadNextSong();
                }

                if (ps->songHasErrors)
                        tryLoadNext();

                if (opsIsDone())
                {
                        prepareNextSong();
                        playbackSwitchDecoder();
                }
        }
        else
        {
                opsSetEofHandled();
        }
}

gboolean mainloopCallback(gpointer data)
{
        (void)data;

        calcElapsedTime(getCurrentSongDuration());
        handleInput();
        incrementUpdateCounter();

        int updateCounter = getUpdateCounter();

        // Different views run at different speeds to lower the impact on system
        // requirements
        if ((updateCounter % 2 == 0 && statePtr->currentView == SEARCH_VIEW) ||
            (statePtr->currentView == TRACK_VIEW || updateCounter % 3 == 0))
        {
                processDBusEvents();

                updateAndLoad();
        }

        return TRUE;
}

static gboolean quitOnSignal(gpointer user_data)
{
        GMainLoop *loop = (GMainLoop *)user_data;
        g_main_loop_quit(loop);
        quit();
        return G_SOURCE_REMOVE; // Remove the signal source
}

void createLoop(void)
{
        updateLastInputTime();

        GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);

        g_unix_signal_add(SIGINT, quitOnSignal, main_loop);
        g_unix_signal_add(SIGHUP, quitOnSignal, main_loop);
        g_timeout_add(34, mainloopCallback, NULL);
        g_main_loop_run(main_loop);
        g_main_loop_unref(main_loop);
}

void run(bool startPlaying)
{
        AppState *state = getAppState();
        PlayList *playlist = getPlaylist();
        PlayList *unshuffledPlaylist = getUnshuffledPlaylist();
        PlaybackState *ps = getPlaybackState();
        UserData *userData = audioData.pUserData;

        if (unshuffledPlaylist == NULL)
        {
                setUnshuffledPlaylist(deepCopyPlayList(playlist));
        }

        if (state->uiSettings.saveRepeatShuffleSettings)
        {
                if (state->uiSettings.repeatState == 1)
                        toggleRepeat();
                if (state->uiSettings.repeatState == 2)
                {
                        toggleRepeat();
                        toggleRepeat();
                }
                if (state->uiSettings.shuffleEnabled)
                        toggleShuffle();
        }

        if (playlist->head == NULL)
        {
                state->currentView = LIBRARY_VIEW;
        }

        initMpris();

        ps->loadedNextSong = false;
        if (startPlaying)
                ps->waitingForPlaylist = true;

        audioData.currentFileIndex = 0;
        userData->currentSongData = NULL;
        userData->songdataA = NULL;
        userData->songdataB = NULL;
        userData->songdataADeleted = true;
        userData->songdataBDeleted = true;

        if (playlist->count != 0)
                checkAndLoadNextSong();

        createLoop();

        clearScreen();
        fflush(stdout);
}

void initDefaultState(void)
{
        init();

        AppState *state = getAppState();
        FileSystemEntry *library = getLibrary();
        PlayList *playlist = getPlaylist();
        PlayList *unshuffledPlaylist = getUnshuffledPlaylist();
        PlaybackState *ps = getPlaybackState();

        loadLastUsedPlaylist(playlist, &unshuffledPlaylist);
        setUnshuffledPlaylist(unshuffledPlaylist);
        markListAsEnqueued(library, playlist);
        resetListAfterDequeuingPlayingSong();

        audioData.restart = true;
        audioData.endOfListReached = true;
        ps->loadedNextSong = false;

        state->currentView = LIBRARY_VIEW;

        run(false);
}

void kewShutdown()
{
        AppState *state = getAppState();
        PlaybackState *ps = getPlaybackState();
        FileSystemEntry *library = getLibrary();
        AppSettings *settings = getAppSettings();
        PlayList *unshuffledPlaylist = getUnshuffledPlaylist();
        PlayList *favoritesPlaylist = getFavoritesPlaylist();

        pthread_mutex_lock(&(state->dataSourceMutex));

        playbackFreeDecoders();

        playbackShutdown();

        emitPlaybackStoppedMpris();

        bool noMusicFound = false;

        if (library == NULL || library->children == NULL)
        {
                noMusicFound = true;
        }

        UserData *userData = audioData.pUserData;

        playbackUnloadSongs(userData);

#ifdef CHAFA_VERSION_1_16
        retire_passthrough_workarounds_tmux();
#endif

        freeSearchResults();
        cleanupMpris();
        restoreTerminalMode();
        enableInputBuffering();
        setPath(settings->path);
        setPrefs(settings, &(state->uiSettings));
        saveFavoritesPlaylist(settings->path, favoritesPlaylist);
        saveLastUsedPlaylist(unshuffledPlaylist);
        deleteCache(statePtr->tmpCache);
        freeMainDirectoryTree();
        freePlaylists();
        setDefaultTextColor();

        if (audioData.pUserData != NULL)
                free(audioData.pUserData);

        pthread_mutex_destroy(&(ps->loadingdata.mutex));
        pthread_mutex_destroy(&(state->switchMutex));
        pthread_mutex_unlock(&(state->dataSourceMutex));
        pthread_mutex_destroy(&(state->dataSourceMutex));

        freeVisuals();

#ifdef USE_DBUS
        cleanupNotifications();
#endif

#ifdef DEBUG
        if (state->uiState.logFile)
                fclose(state->uiState.logFile);
#endif

        if (freopen("/dev/stderr", "w", stderr) == NULL)
        {
                perror("freopen error");
        }

        printf("\n");
        showCursor();
        exitAlternateScreenBuffer();

        if (statePtr->uiSettings.mouseEnabled)
                disableTerminalMouseButtons();

        if (statePtr->uiSettings.trackTitleAsWindowTitle)
                restoreTerminalWindowTitle();

        if (noMusicFound)
        {
                printf("No Music found.\n");
                printf("Please make sure the path is set correctly. \n");
                printf("To set it type: kew path \"/path/to/Music\". \n");
        }
        else if (state->uiState.noPlaylist)
        {
                printf("Music not found.\n");
        }

        if (hasErrorMessage())
        {
                printf("%s\n", getErrorMessage());
        }

        fflush(stdout);
}

void initState(void)
{
        AppState *state = getAppState();

        state->uiSettings.VERSION = VERSION;
        state->uiSettings.uiEnabled = true;
        state->uiSettings.color.r = 125;
        state->uiSettings.color.g = 125;
        state->uiSettings.color.b = 125;
        state->uiSettings.coverEnabled = true;
        state->uiSettings.hideLogo = false;
        state->uiSettings.hideHelp = false;
        state->uiSettings.quitAfterStopping = false;
        state->uiSettings.hideGlimmeringText = false;
        state->uiSettings.coverAnsi = false;
        state->uiSettings.visualizerEnabled = true;
        state->uiSettings.visualizerHeight = 5;
        state->uiSettings.visualizerColorType = 0;
        state->uiSettings.visualizerBrailleMode = false;
        state->uiSettings.visualizerBarWidth = 2;
        state->uiSettings.titleDelay = 9;
        state->uiSettings.cacheLibrary = -1;
        state->uiSettings.mouseEnabled = true;
        state->uiSettings.mouseLeftClickAction = 0;
        state->uiSettings.mouseMiddleClickAction = 1;
        state->uiSettings.mouseRightClickAction = 2;
        state->uiSettings.mouseScrollUpAction = 3;
        state->uiSettings.mouseScrollDownAction = 4;
        state->uiSettings.mouseAltScrollUpAction = 7;
        state->uiSettings.mouseAltScrollDownAction = 8;
        state->uiSettings.replayGainCheckFirst = 0;
        state->uiSettings.saveRepeatShuffleSettings = 1;
        state->uiSettings.repeatState = 0;
        state->uiSettings.shuffleEnabled = 0;
        state->uiSettings.trackTitleAsWindowTitle = 1;
        state->uiState.numDirectoryTreeEntries = 0;
        state->uiState.numProgressBars = 35;
        state->uiState.chosenNodeId = 0;
        state->uiState.resetPlaylistDisplay = true;
        state->uiState.allowChooseSongs = false;
        state->uiState.openedSubDir = false;
        state->uiState.numSongsAboveSubDir = 0;
        state->uiState.resizeFlag = 0;
        state->uiState.collapseView = false;
        state->uiState.refresh = true;
        state->uiState.isFastForwarding = false;
        state->uiState.isRewinding = false;
        state->uiState.songWasRemoved = false;
        state->uiState.startFromTop = false;
        state->uiState.lastNotifiedId = -1;
        state->uiState.noPlaylist = false;
        state->uiState.logFile = NULL;
        state->uiState.showLyricsPage = false;
        state->uiState.currentLibEntry = NULL;
        state->tmpCache = NULL;
        state->uiSettings.LAST_ROW = " [F2 Playlist|F3 Library|F4 Track|F5 Search|F6 Help]";
        state->uiSettings.defaultColor = 150;
        state->uiSettings.defaultColorRGB.r = state->uiSettings.defaultColor;
        state->uiSettings.defaultColorRGB.g = state->uiSettings.defaultColor;
        state->uiSettings.defaultColorRGB.b = state->uiSettings.defaultColor;
        state->uiSettings.kewColorRGB.r = 222;
        state->uiSettings.kewColorRGB.g = 43;
        state->uiSettings.kewColorRGB.b = 77;

        PlaybackState *ps = getPlaybackState();

        ps->lastPlayedId = -1;
        ps->usingSongDataA = true;
        ps->nextSongNeedsRebuilding = false;
        ps->songHasErrors = false;
        ps->forceSkip = false;
        ps->skipFromStopped = false;
        ps->skipping = false;
        ps->skipOutOfOrder = false;
        ps->clearingErrors = false;
        ps->hasSilentlySwitched = false;
        ps->songLoading = false;
        ps->loadedNextSong = false;
        ps->waitingForNext = false;
        ps->waitingForPlaylist = false;
        ps->notifySwitch = false;
        ps->notifyPlaying = false;

        pthread_mutex_init(&(state->dataSourceMutex), NULL);
        pthread_mutex_init(&(state->switchMutex), NULL);

        setUnshuffledPlaylist(NULL);
        setFavoritesPlaylist(NULL);

        audioData.pUserData = malloc(sizeof(UserData));

        statePtr = state;
}

int main(int argc, char *argv[])
{
        AppState *state = getAppState();

        initState();
        restartIfAlreadyRunning(argv);

        AppSettings *settings = getAppSettings();
        PlayList *playlist = getPlaylist();
        PlayList *favoritesPlaylist = getFavoritesPlaylist();

        if ((argc == 2 &&
             ((strcmp(argv[1], "--help") == 0) ||
              (strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "-?") == 0))))
        {
                showHelp();
                exit(0);
        }
        else if (argc == 2 && (strcmp(argv[1], "--version") == 0 ||
                               strcmp(argv[1], "-v") == 0))
        {
                state->uiSettings.colorMode = COLOR_MODE_ALBUM;
                state->uiSettings.color = state->uiSettings.defaultColorRGB;
                printAbout(NULL);
                exit(0);
        }

        *settings = initSettings();
        transferSettingsToUI();
        initKeyMappings(settings);
        setTrackTitleAsWindowTitle();

        if (argc == 3 && (strcmp(argv[1], "path") == 0))
        {
                char deExpanded[MAXPATHLEN];
                collapsePath(argv[2], deExpanded);
                c_strcpy(settings->path, deExpanded, sizeof(settings->path));
                setPath(settings->path);
                exit(0);
        }

        enableMouse(&(state->uiSettings));
        enterAlternateScreenBuffer();
        atexit(kewShutdown);

        if (settings->path[0] == '\0')
        {
                setMusicPath();
        }

        bool exactSearch = false;
        handleOptions(&argc, argv, &exactSearch);
        loadFavoritesPlaylist(settings->path, &favoritesPlaylist);

        ensureDefaultThemePack();

        initTheme(argc, argv);

        if (argc == 1)
        {
                initDefaultState();
        }
        else if (argc == 2 && strcmp(argv[1], "all") == 0)
        {
                init();
                playAll();
                run(true);
        }
        else if (argc == 2 && strcmp(argv[1], "albums") == 0)
        {
                init();
                playAllAlbums();
                run(true);
        }
        else if (argc == 2 && strcmp(argv[1], ".") == 0 && favoritesPlaylist->count != 0)
        {
                init();
                playFavoritesPlaylist();
                run(true);
        }
        else if (argc >= 2)
        {
                init();
                makePlaylist(&playlist, argc, argv, exactSearch, settings->path);

                if (playlist->count == 0)
                {
                        if (strcmp(argv[1], "theme") != 0)
                                state->uiState.noPlaylist = true;
                        exit(0);
                }

                FileSystemEntry *library = getLibrary();

                markListAsEnqueued(library, playlist);
                run(true);
        }

        return 0;
}
