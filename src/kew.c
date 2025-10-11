/* kew - A terminal music player
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

#include "appstate.h"
#include "cache.h"
#include "common.h"
#include "common_ui.h"
#include "events.h"
#include "imgfunc.h"
#include "input.h"
#include "mpris.h"
#include "notifications.h"
#include "player_ui.h"
#include "playerops.h"
#include "playlist.h"
#include "search_ui.h"
#include "settings.h"
#include "songloader.h"
#include "sound.h"
#include "soundcommon.h"
#include "term.h"
#include "theme.h"
#include "utils.h"
#include "visuals.h"
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
#include <time.h>
#include <unistd.h>

const char VERSION[] = "3.5.3";

AppState *statePtr = NULL;

void setEndOfListReached(AppState *state)
{
        AudioData *audioData = getAudioData();

        state->uiState.loadedNextSong = false;
        audioData->endOfListReached = true;

        setIsUsingSongDataA(false);

        audioData->currentFileIndex = 0;
        audioData->restart = true;

        LoadingThreadData *loadingdata = getLoadingData();

        loadingdata->loadA = true;
        state->uiState.waitingForNext = true;

        clearCurrentSong();

        pthread_mutex_lock(&(state->dataSourceMutex));
        cleanupPlaybackDevice();
        pthread_mutex_unlock(&(state->dataSourceMutex));
        setStopped(true);
        triggerRefresh();

        if (isRepeatListEnabled())
                repeatList(state);
        else
        {
                emitPlaybackStoppedMpris();
                emitMetadataChanged("", "", "", "",
                                    "/org/mpris/MediaPlayer2/TrackList/NoTrack",
                                    NULL, 0);
                state->currentView = LIBRARY_VIEW;
                clearScreen();
        }
}

void notifyMPRISSwitch(SongData *currentSongData)
{
        if (currentSongData == NULL)
                return;

        gint64 length = getLengthInMicroSec(currentSongData->duration);

        // Update mpris
        emitMetadataChanged(
            currentSongData->metadata->title, currentSongData->metadata->artist,
            currentSongData->metadata->album, currentSongData->coverArtPath,
            currentSongData->trackId != NULL ? currentSongData->trackId : "",
            getCurrentSong(), length);
}

void notifySongSwitch(SongData *currentSongData, AppState *state)
{
        UISettings *ui = &(state->uiSettings);
        if (currentSongData != NULL && currentSongData->hasErrors == 0 &&
            currentSongData->metadata &&
            strnlen(currentSongData->metadata->title, 10) > 0)
        {
#ifdef USE_DBUS
                displaySongNotification(currentSongData->metadata->artist,
                                        currentSongData->metadata->title,
                                        currentSongData->coverArtPath, ui);
#else
                (void)ui;
#endif

                notifyMPRISSwitch(currentSongData);

                Node *current = getCurrentSong();

                if (current != NULL)
                        state->uiState.lastNotifiedId = current->id;
        }
}

void determineSongAndNotify(AppState *state)
{
        SongData *currentSongData = NULL;

        bool isDeleted = determineCurrentSongData(&currentSongData);

        Node *current = getCurrentSong();

        if (currentSongData && current)
                current->song.duration = currentSongData->duration;

        if (state->uiState.lastNotifiedId != current->id)
        {
                if (!isDeleted)
                        notifySongSwitch(currentSongData, state);
        }
}

// Refreshes the player visually if conditions are met
void refreshPlayer(AppState *state)
{
        UIState *uis = &(state->uiState);
        AppSettings *settings = getAppSettings();

        int mutexResult = pthread_mutex_trylock(&(state->switchMutex));

        if (mutexResult != 0)
        {
                fprintf(stderr, "Failed to lock switch mutex.\n");
                return;
        }

        if (uis->doNotifyMPRISPlaying)
        {
                uis->doNotifyMPRISPlaying = false;

                emitStringPropertyChanged("PlaybackStatus", "Playing");
        }

        if (uis->doNotifyMPRISSwitched)
        {
                uis->doNotifyMPRISSwitched = false;

                notifyMPRISSwitch(getCurrentSongData());
        }

        if (shouldRefreshPlayer())
        {
                printPlayer(getCurrentSongData(), getElapsedSeconds(), settings,
                            state);
        }

        pthread_mutex_unlock(&(state->switchMutex));
}

void resetListAfterDequeuingPlayingSong(AppState *state)
{
        state->uiState.startFromTop = true;

        if (getLastPlayedId() < 0)
                return;

        Node *node = findSelectedEntryById(getPlaylist(), getLastPlayedId());

        if (getCurrentSong() == NULL && node == NULL)
        {
                stopPlayback(state);

                AudioData *audioData = getAudioData();

                state->uiState.loadedNextSong = false;
                audioData->endOfListReached = true;
                audioData->restart = true;

                emitMetadataChanged("", "", "", "",
                                    "/org/mpris/MediaPlayer2/TrackList/NoTrack",
                                    NULL, 0);
                emitPlaybackStoppedMpris();

                pthread_mutex_lock(&(state->dataSourceMutex));
                cleanupPlaybackDevice();
                pthread_mutex_unlock(&(state->dataSourceMutex));

                triggerRefresh();

                switchAudioImplementation(state);

                unloadSongA(state);
                unloadSongB(state);
                state->uiState.songWasRemoved = true;

                UserData *userData = getUserData();

                userData->currentSongData = NULL;

                audioData->currentFileIndex = 0;
                audioData->restart = true;
                state->uiState.waitingForNext = true;

                LoadingThreadData *loadingdata = getLoadingData();

                loadingdata->loadA = true;
                setIsUsingSongDataA(false);

                ma_data_source_uninit(&audioData);

                audioData->switchFiles = false;

                if (getPlaylist()->count == 0)
                        setSongToStartFrom(NULL);
        }
}

int getSongNumber(const char *str)
{
        char *endptr;
        long value = strtol(str, &endptr, 10);

        if (*endptr != '\0')
        {
                return 0;
        }

        if (value < 0 || value > INT_MAX)
        {
                return 0;
        }

        return (int)value;
}

FileSystemEntry *enqueue(AppState *state, FileSystemEntry *entry)
{
        FileSystemEntry *firstEnqueuedEntry = NULL;

        AudioData *audioData = getAudioData();

        if (audioData->restart)
        {
                Node *lastSong = findSelectedEntryById(getPlaylist(), getLastPlayedId());
                state->uiState.startFromTop = false;

                if (lastSong == NULL)
                {
                        if (getPlaylist()->tail != NULL)
                                setLastPlayedId(getPlaylist()->tail->id);
                        else
                        {
                                setLastPlayedId(-1);
                                state->uiState.startFromTop = true;
                        }
                }
        }

        pthread_mutex_lock(&(getPlaylist()->mutex));

        firstEnqueuedEntry = enqueueSongs(entry, state);
        resetListAfterDequeuingPlayingSong(state);

        pthread_mutex_unlock(&(getPlaylist()->mutex));

        return firstEnqueuedEntry;
}

bool playPreProcessing()
{
        bool wasEndOfList = false;

        AudioData *audioData = getAudioData();

        if (audioData->endOfListReached)
                wasEndOfList = true;

        return wasEndOfList;
}

void playPostProcessing(bool wasEndOfList, AppState *state)
{
        if ((state->uiState.songWasRemoved && getCurrentSong() != NULL))
        {
                state->uiState.songWasRemoved = false;
        }

        if (wasEndOfList)
        {
                setSkipOutOfOrder(false);
        }

        AudioData *audioData = getAudioData();

        audioData->endOfListReached = false;
}

FileSystemEntry *libraryEnqueue(AppState *state, PlayList *playlist)
{
        FileSystemEntry *entry = getCurrentLibEntry();
        FileSystemEntry *firstEnqueuedEntry = NULL;

        if (entry == NULL)
                return NULL;

        // Enqueue playlist
        if (pathEndsWith(entry->fullPath, "m3u") ||
            pathEndsWith(entry->fullPath, "m3u8"))
        {
                Node *prevTail = playlist->tail;

                readM3UFile(entry->fullPath, playlist, getLibrary());

                if (prevTail != NULL && prevTail->next != NULL)
                {
                        firstEnqueuedEntry = findCorrespondingEntry(
                            getLibrary(), prevTail->next->song.filePath);
                }
                else if (playlist->head != NULL)
                {
                        firstEnqueuedEntry = findCorrespondingEntry(
                            getLibrary(), playlist->head->song.filePath);
                }

                autostartIfStopped(firstEnqueuedEntry, &(state->uiState));

                markListAsEnqueued(getLibrary(), playlist);

                PlayList *unshuffledPlaylist = getUnshuffledPlaylist();

                deepCopyPlayListOntoList(playlist, &unshuffledPlaylist);
                shufflePlaylist(playlist);
                setUnshuffledPlaylist(unshuffledPlaylist);
        }
        else
        {
                firstEnqueuedEntry = enqueue(state, entry); // Enqueue song
        }

        return firstEnqueuedEntry;
}

FileSystemEntry *searchEnqueue(AppState *state, PlayList *playlist)
{
        pthread_mutex_lock(&(playlist->mutex));

        FileSystemEntry *entry = getCurrentSearchEntry();

        state->uiState.waitingForPlaylist = false;

        setChosenDir(entry);

        FileSystemEntry *firstEnqueuedEntry = enqueueSongs(entry, state);

        resetListAfterDequeuingPlayingSong(state);

        pthread_mutex_unlock(&(playlist->mutex));

        return firstEnqueuedEntry;
}

void clearAndPlay(AppState *state, Node *song)
{
        cleanupPlaybackDevice();
        state->uiState.loadedNextSong = true;

        setNextSongNeedsRebuilding(false);

        unloadSongA(state);
        unloadSongB(state);

        setIsUsingSongDataA(false);

        AudioData *audioData = getAudioData();

        audioData->currentFileIndex = 0;

        LoadingThreadData *loadingdata = getLoadingData();

        loadingdata->loadA = true;

        bool wasEndOfList = playPreProcessing();

        playbackPlay(state);

        play(song, state);

        playPostProcessing(wasEndOfList, state);

        setSkipOutOfOrder(false);
        setIsUsingSongDataA(true);
}

void playlistEnqueue(AppState *state, PlayList *playlist)
{
        if (!isDigitsPressed())
        {
                Node *current = getCurrentSong();

                if (isPaused() && current != NULL &&
                    state->uiState.chosenNodeId == current->id)
                {
                        togglePause(state);
                }
                else
                {
                        Node *song = NULL;
                        findNodeInList(playlist,
                                       state->uiState.chosenNodeId,
                                       &song);

                        clearAndPlay(state, song);
                }
        }
        else
        {
                state->uiState.resetPlaylistDisplay = true;
                int songNumber = getSongNumber(getDigitsPressed());

                resetDigitsPressed();

                setNextSongNeedsRebuilding(false);

                skipToNumberedSong(songNumber, state);
        }
}

FileSystemEntry *viewEnqueue(AppState *state, PlayList *playlist)
{
        FileSystemEntry *firstEnqueuedEntry = NULL;

        if (state->currentView == LIBRARY_VIEW)
        {
                firstEnqueuedEntry = libraryEnqueue(state, playlist);
        }

        else if (state->currentView == SEARCH_VIEW)
        {
                firstEnqueuedEntry = searchEnqueue(state, playlist);
        }
        else if (state->currentView == PLAYLIST_VIEW)
        {
                playlistEnqueue(state, playlist);
        }

        return firstEnqueuedEntry;
}

void enqueueHandler(AppState *state, bool playImmediately)
{
        PlayList *playlist = getPlaylist();
        Node *current = getCurrentSong();
        bool wasEmpty = (playlist->count == 0);
        bool canGoNextBefore = (current != NULL && current->next != NULL);

        FileSystemEntry *firstEnqueuedEntry = viewEnqueue(state, playlist);

        if (playImmediately && firstEnqueuedEntry && !wasEmpty)
        {
                Node *song = findPathInPlaylist(firstEnqueuedEntry->fullPath, playlist);
                clearAndPlay(state, song);
        }

        Node *currentAfter = getCurrentSong();
        bool canGoNextAfter = (currentAfter != NULL && currentAfter->next != NULL);

        if (canGoNextBefore != canGoNextAfter)
        {
                emitBooleanPropertyChanged("CanGoNext", canGoNextAfter);
        }
}

void gotoBeginningOfPlaylist(AppState *state)
{
        pressDigit(1);
               enqueueHandler(state, false);
}

void gotoEndOfPlaylist(AppState *state)
{
        if (isDigitsPressed())
        {
                enqueueHandler(state, false);
        }
        else
        {
                skipToLastSong(state);
        }
}

void handleInput(AppState *state)
{
        struct Event event = processInput(state);

        AppSettings *settings = getAppSettings();
        PlayList *playlist = getPlaylist();

        switch (event.type)
        {
        case EVENT_GOTOBEGINNINGOFPLAYLIST:
                gotoBeginningOfPlaylist(state);
                break;
        case EVENT_GOTOENDOFPLAYLIST:
                gotoEndOfPlaylist(state);
                break;
        case EVENT_ENQUEUE:
                enqueueHandler(state, false);
                break;
        case EVENT_ENQUEUEANDPLAY:
                enqueueHandler(state, true);
                break;
        case EVENT_PLAY_PAUSE:
                if (isStopped())
                {
                        enqueueHandler(state, false);
                }
                else
                {
                        togglePause(state);
                }
                break;
        case EVENT_TOGGLEVISUALIZER:
                toggleVisualizer(settings, &(state->uiSettings));
                break;
        case EVENT_TOGGLEREPEAT:
                toggleRepeat(state);
                break;
        case EVENT_TOGGLEASCII:
                toggleAscii(settings, &(state->uiSettings));
                break;
        case EVENT_SHUFFLE:
                toggleShuffle(state);
                emitShuffleChanged();
                break;
        case EVENT_CYCLECOLORMODE:
                cycleColorMode(state);
                break;
        case EVENT_CYCLETHEMES:
                cycleThemes(state, settings);
                break;
        case EVENT_TOGGLENOTIFICATIONS:
                toggleNotifications(&(state->uiSettings), settings);
                break;
        case EVENT_QUIT:
                quit();
                break;
        case EVENT_SCROLLNEXT:
                scrollNext(state);
                break;
        case EVENT_SCROLLPREV:
                scrollPrev(state);
                break;
        case EVENT_VOLUME_UP:
                adjustVolumePercent(5);
                emitVolumeChanged();
                break;
        case EVENT_VOLUME_DOWN:
                adjustVolumePercent(-5);
                emitVolumeChanged();
                break;
        case EVENT_NEXT:
                skipToNextSong(state);
                break;
        case EVENT_PREV:
                skipToPrevSong(state);
                break;
        case EVENT_SEEKBACK:
                seekBack(&(state->uiState));
                break;
        case EVENT_SEEKFORWARD:
                seekForward(&(state->uiState));
                break;
        case EVENT_ADDTOFAVORITESPLAYLIST:
                addToFavoritesPlaylist();
                break;
        case EVENT_EXPORTPLAYLIST:
                exportCurrentPlaylist(settings->path, playlist);
                break;
        case EVENT_UPDATELIBRARY:
                updateLibrary(state, settings->path);
                break;
        case EVENT_SHOWKEYBINDINGS:
                toggleShowView(KEYBINDINGS_VIEW, state);
                break;
        case EVENT_SHOWPLAYLIST:
                toggleShowView(PLAYLIST_VIEW, state);
                break;
        case EVENT_SHOWSEARCH:
                toggleShowView(SEARCH_VIEW, state);
                break;
                break;
        case EVENT_SHOWLIBRARY:
                toggleShowView(LIBRARY_VIEW, state);
                break;
        case EVENT_NEXTPAGE:
                flipNextPage(state);
                break;
        case EVENT_PREVPAGE:
                flipPrevPage(state);
                break;
        case EVENT_REMOVE:
                handleRemove(state);
                resetListAfterDequeuingPlayingSong(state);
                break;
        case EVENT_SHOWTRACK:
                showTrack(state);
                break;
        case EVENT_NEXTVIEW:
                switchToNextView(state);
                break;
        case EVENT_PREVVIEW:
                switchToPreviousView(state);
                break;
        case EVENT_CLEARPLAYLIST:
                updatePlaylistToPlayingSong(state);
                state->uiState.resetPlaylistDisplay = true;
                break;
        case EVENT_MOVESONGUP:
                moveSongUp(state);
                break;
        case EVENT_MOVESONGDOWN:
                moveSongDown(state);
                break;
        case EVENT_STOP:
                stop(state);
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

void resize(UIState *uis)
{
        alarm(1); // Timer
        while (uis->resizeFlag)
        {
                uis->resizeFlag = 0;
                c_sleep(100);
        }
        alarm(0); // Cancel timer
        printf("\033[1;1H");
        clearScreen();
        triggerRefresh();
}

void updatePlayer(AppState *state)
{
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
                refreshPlayer(state);
        }
}

void loadAudioData(AppState *state)
{
        AudioData *audioData = getAudioData();

        if (audioData->restart == true)
        {
                PlayList *playlist = getPlaylist();

                if (playlist->head != NULL &&
                    (state->uiState.waitingForPlaylist || state->uiState.waitingForNext))
                {
                        setSongLoading(true);

                        if (state->uiState.waitingForPlaylist)
                        {
                                setCurrentSong(playlist->head);
                        }
                        else if (state->uiState.waitingForNext)
                        {
                                Node *current = NULL;
                                Node *songToStartFrom = getSongToStartFrom();

                                if (songToStartFrom != NULL)
                                {
                                        // Make sure it still exists in the
                                        // playlist
                                        findNodeInList(playlist,
                                                       songToStartFrom->id,
                                                       &current);

                                        if (current != NULL)
                                        {
                                                setCurrentSong(current);
                                        }
                                        songToStartFrom = NULL;
                                }
                                else if (getLastPlayedId() >= 0)
                                {
                                        current = findSelectedEntryById(
                                            playlist, getLastPlayedId());

                                        if (current != NULL)
                                        {
                                                if (current->next != NULL)
                                                        current = current->next;

                                                setCurrentSong(current);
                                        }
                                }

                                if (current == NULL)
                                {
                                        if (state->uiState.startFromTop)
                                        {
                                                setCurrentSong(playlist->head);
                                                state->uiState.startFromTop = false;
                                        }
                                        else
                                                setCurrentSong(playlist->tail);
                                }
                        }

                        audioData->restart = false;
                        state->uiState.waitingForPlaylist = false;
                        state->uiState.waitingForNext = false;
                        state->uiState.songWasRemoved = false;

                        if (isShuffleEnabled())
                                reshufflePlaylist();

                        unloadSongA(state);
                        unloadSongB(state);

                        int res = loadFirst(getCurrentSong(), state);

                        finishLoading(&(state->uiState));

                        if (res >= 0)
                        {
                                res = createAudioDevice(state);
                        }

                        if (res >= 0)
                        {
                                resumePlayback(state);
                        }
                        else
                        {
                                setEndOfListReached(state);
                        }

                        state->uiState.loadedNextSong = false;
                        setNextSong(NULL);
                        triggerRefresh();

                        resetClock();
                }
        }
        else if (getCurrentSong() != NULL &&
                 (getNextSongNeedsRebuilding() || getNextSong() == NULL) && !isSongLoading())
        {
                loadNextSong(state);
                determineSongAndNotify(state);
        }
}

void handleSkipOutOfOrder(void)
{
        if (!isSkipOutOfOrder() && !isRepeatEnabled())
        {
                setCurrentSongToNext();
        }
        else
        {
                setSkipOutOfOrder(false);
        }
}

void prepareNextSong(AppState *state)
{
        resetClock();
        handleSkipOutOfOrder();
        finishLoading(&(state->uiState));

        setNextSong(NULL);
        triggerRefresh();

        Node *current = getCurrentSong();

        if (!isRepeatEnabled() || current == NULL)
        {
                unloadPreviousSong(state);
        }

        if (current == NULL)
        {
                if (state->uiSettings.quitAfterStopping)
                {
                        quit();
                }
                else
                {
                        setEndOfListReached(state);
                }
        }
        else
        {
                determineSongAndNotify(state);
        }
}

void updatePlayerStatus(AppState *state)
{
        updatePlayer(state);

        PlayList *playlist = getPlaylist();
        AudioData *audioData = getAudioData();

        if (playlist->head != NULL)
        {
                if ((hasSkippedFromStopped() || !state->uiState.loadedNextSong ||
                     getNextSongNeedsRebuilding()) &&
                    !audioData->endOfListReached)
                {
                        loadAudioData(state);
                }

                if (hasErrorsSong())
                        tryLoadNext(state);

                if (isPlaybackDone())
                {
                        prepareNextSong(state);
                        switchAudioImplementation(state);
                }
        }
        else
        {
                setEOFNotReached();
        }
}

void processDBusEvents(void)
{
        while (g_main_context_pending(getGMainContext()))
        {
                g_main_context_iteration(getGMainContext(), FALSE);
        }
}

gboolean mainloop_callback(gpointer data)
{
        (void)data;

        calcElapsedTime();
        handleInput(statePtr);
        incrementUpdateCounter();

        int updateCounter = getUpdateCounter();

        // Different views run at different speeds to lower the impact on system
        // requirements
        if ((updateCounter % 2 == 0 && statePtr->currentView == SEARCH_VIEW) ||
            (statePtr->currentView == TRACK_VIEW || updateCounter % 3 == 0))
        {
                processDBusEvents();

                updatePlayerStatus(statePtr);
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

void initFirstPlay(Node *song, AppState *state)
{
        updateLastInputTime();
        resetClock();

        UserData *userData = getUserData();

        userData->currentSongData = NULL;
        userData->songdataA = NULL;
        userData->songdataB = NULL;
        userData->songdataADeleted = true;
        userData->songdataBDeleted = true;

        int res = 0;

        if (song != NULL)
        {
                AudioData *audioData = getAudioData();

                audioData->currentFileIndex = 0;

                LoadingThreadData *loadingdata = getLoadingData();

                loadingdata->loadA = true;
                res = loadFirst(song, state);

                if (res >= 0)
                {
                        res = createAudioDevice(state);
                }
                if (res >= 0)
                {
                        resumePlayback(state);
                }

                if (res < 0)
                        setEndOfListReached(state);
        }

        if (song == NULL || res < 0)
        {
                song = NULL;
                if (!state->uiState.waitingForNext)
                        state->uiState.waitingForPlaylist = true;
        }

        state->uiState.loadedNextSong = false;
        setNextSong(NULL);
        triggerRefresh();

        resetClock();

        GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);

        g_unix_signal_add(SIGINT, quitOnSignal, main_loop);
        g_unix_signal_add(SIGHUP, quitOnSignal, main_loop);

        if (song != NULL)
                emitStartPlayingMpris();
        else
                emitPlaybackStoppedMpris();

        g_timeout_add(34, mainloop_callback, NULL);
        g_main_loop_run(main_loop);
        g_main_loop_unref(main_loop);
}

void cleanupOnExit()
{
        AppState *state = getAppState();
        pthread_mutex_lock(&(state->dataSourceMutex));

        resetAllDecoders();

        if (isContextInitialized())
        {
#ifdef __ANDROID__
                shutdownAndroid();
#else
                cleanupPlaybackDevice();
#endif
                cleanupAudioContext();
        }

        emitPlaybackStoppedMpris();

        bool noMusicFound = false;

        FileSystemEntry *library = getLibrary();

        if (library == NULL || library->children == NULL)
        {
                noMusicFound = true;
        }

        LoadingThreadData *loadingdata = getLoadingData();
        UserData *userData = getUserData();

        if (!userData->songdataADeleted)
        {
                userData->songdataADeleted = true;
                unloadSongData(&(loadingdata->songdataA), statePtr);
        }
        if (!userData->songdataBDeleted)
        {
                userData->songdataBDeleted = true;
                unloadSongData(&(loadingdata->songdataB), statePtr);
        }

#ifdef CHAFA_VERSION_1_16
        retire_passthrough_workarounds_tmux();
#endif

        AppSettings *settings = getAppSettings();
        PlayList *playlist = getPlaylist();
        PlayList *unshuffledPlaylist = getUnshuffledPlaylist();
        PlayList *favoritesPlaylist = getFavoritesPlaylist();

        freeSearchResults();
        cleanupMpris();
        restoreTerminalMode();
        enableInputBuffering();
        setConfig(settings, &(statePtr->uiSettings));
        saveFavoritesPlaylist(settings->path, favoritesPlaylist);
        saveLastUsedPlaylist(unshuffledPlaylist);
        deleteCache(statePtr->tmpCache);
        freeMainDirectoryTree(statePtr);
        freePlaylists();
        setDefaultTextColor();
        pthread_mutex_destroy(&(loadingdata->mutex));
        pthread_mutex_destroy(&(playlist->mutex));
        pthread_mutex_destroy(&(state->switchMutex));
        pthread_mutex_unlock(&(state->dataSourceMutex));
        pthread_mutex_destroy(&(state->dataSourceMutex));
        freeVisuals();

#ifdef USE_DBUS
        cleanupDbusConnection();
#endif

#ifdef DEBUG
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

void run(AppState *state, bool startPlaying)
{
        PlayList *playlist = getPlaylist();

        PlayList *unshuffledPlaylist = getUnshuffledPlaylist();

        if (unshuffledPlaylist == NULL)
        {
                setUnshuffledPlaylist(deepCopyPlayList(playlist));
        }

        if (state->uiSettings.saveRepeatShuffleSettings)
        {
                if (state->uiSettings.repeatState == 1)
                        toggleRepeat(state);
                if (state->uiSettings.repeatState == 2)
                {
                        toggleRepeat(state);
                        toggleRepeat(state);
                }
                if (state->uiSettings.shuffleEnabled)
                        toggleShuffle(state);
        }

        if (playlist->head == NULL)
        {
                state->currentView = LIBRARY_VIEW;
        }

        initMpris(state);

        if (startPlaying)
                setCurrentSong(playlist->head);
        else if (playlist->count > 0)
                state->uiState.waitingForNext = true;

        initFirstPlay(getCurrentSong(), state);
        clearScreen();
        fflush(stdout);
}

void handleResize(int sig)
{
        (void)sig;
        statePtr->uiState.resizeFlag = 1;
}

void resetResizeFlag(int sig)
{
        (void)sig;
        statePtr->uiState.resizeFlag = 0;
}

void initResize()
{
        signal(SIGWINCH, handleResize);

        struct sigaction sa;
        sa.sa_handler = resetResizeFlag;
        sigemptyset(&(sa.sa_mask));
        sa.sa_flags = 0;
        sigaction(SIGALRM, &sa, NULL);
}

void init(AppState *state)
{
        disableTerminalLineInput();
        setRawInputMode();
        initResize();
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &state->uiState.windowSize);
        enableScrolling();
        setNonblockingMode();
        state->tmpCache = createCache();

        LoadingThreadData *loadingdata = getLoadingData();
        UserData *userData = getUserData();
        AppSettings *settings = getAppSettings();
        PlayList *playlist = getPlaylist();
        AudioData *audioData = getAudioData();

        c_strcpy(loadingdata->filePath, "", sizeof(loadingdata->filePath));
        loadingdata->songdataA = NULL;
        loadingdata->songdataB = NULL;
        loadingdata->loadA = true;
        loadingdata->loadingFirstDecoder = true;
        audioData->restart = true;
        userData->songdataADeleted = true;
        userData->songdataBDeleted = true;
        unsigned int seed = (unsigned int)time(NULL);
        srand(seed);
        pthread_mutex_init(&(loadingdata->mutex), NULL);
        pthread_mutex_init(&(playlist->mutex), NULL);
        createLibrary(settings, state);
        setlocale(LC_ALL, "");
        setlocale(LC_CTYPE, "");
        fflush(stdout);

#ifdef DEBUG
        // g_setenv("G_MESSAGES_DEBUG", "all", TRUE);
        state->uiState.logFile = freopen("error.log", "w", stderr);
        if (state->uiState.logFile == NULL)
        {
                fprintf(stdout, "Failed to redirect stderr to error.log\n");
        }
#else
        FILE *nullStream = freopen("/dev/null", "w", stderr);
        (void)nullStream;
#endif
}

void initDefaultState(AppState *state)
{
        init(state);

        FileSystemEntry *library = getLibrary();
        PlayList *playlist = getPlaylist();
        PlayList *unshuffledPlaylist = getUnshuffledPlaylist();

        loadLastUsedPlaylist(playlist, &unshuffledPlaylist);
        setUnshuffledPlaylist(unshuffledPlaylist);

        markListAsEnqueued(library, playlist);

        resetListAfterDequeuingPlayingSong(state);

        AudioData *audioData = getAudioData();

        audioData->restart = true;
        audioData->endOfListReached = true;
        state->uiState.loadedNextSong = false;

        state->currentView = LIBRARY_VIEW;

        run(state, false);
}

void playFavoritesPlaylist(AppState *state)
{
        PlayList *playlist = getPlaylist();
        PlayList *favoritesPlaylist = getFavoritesPlaylist();

        if (favoritesPlaylist->count == 0)
        {
                printf("Couldn't find any songs in the special playlist. Add a "
                       "song by pressing '.' while it's playing. \n");
                exit(0);
        }

        init(state);

        FileSystemEntry *library = getLibrary();

        deepCopyPlayListOntoList(favoritesPlaylist, &playlist);
        shufflePlaylist(playlist);
        setPlaylist(playlist);
        markListAsEnqueued(library, playlist);

        run(state, true);
}

void playAll(AppState *state)
{
        init(state);

        FileSystemEntry *library = getLibrary();
        PlayList *playlist = getPlaylist();

        createPlayListFromFileSystemEntry(library, playlist, MAX_FILES);

        if (playlist->count == 0)
        {
                exit(0);
        }

        shufflePlaylist(playlist);
        markListAsEnqueued(library, playlist);
        run(state, true);
}

void playAllAlbums(AppState *state)
{
        init(state);

        PlayList *playlist = getPlaylist();
        FileSystemEntry *library = getLibrary();
        addShuffledAlbumsToPlayList(library, playlist, MAX_FILES);

        if (playlist->count == 0)
        {
                exit(0);
        }

        markListAsEnqueued(library, playlist);
        run(state, true);
}

void removeArgElement(char *argv[], int index, int *argc)
{
        if (index < 0 || index >= *argc)
        {
                // Invalid index
                return;
        }

        // Shift elements after the index
        for (int i = index; i < *argc - 1; i++)
        {
                argv[i] = argv[i + 1];
        }

        // Update the argument count
        (*argc)--;
}

void handleOptions(int *argc, char *argv[], UISettings *ui, bool *exactSearch)
{
        const char *noUiOption = "--noui";
        const char *noCoverOption = "--nocover";
        const char *quitOnStop = "--quitonstop";
        const char *quitOnStop2 = "-q";
        const char *exactOption = "--exact";
        const char *exactOption2 = "-e";

        int maxLen = 1000;

        int idx = -1;
        for (int i = 0; i < *argc; i++)
        {
                if (c_strcasestr(argv[i], noUiOption, maxLen))
                {
                        ui->uiEnabled = false;
                        idx = i;
                }
        }
        if (idx >= 0)
                removeArgElement(argv, idx, argc);

        idx = -1;
        for (int i = 0; i < *argc; i++)
        {
                if (c_strcasestr(argv[i], noCoverOption, maxLen))
                {
                        ui->coverEnabled = false;
                        idx = i;
                }
        }
        if (idx >= 0)
                removeArgElement(argv, idx, argc);

        idx = -1;
        for (int i = 0; i < *argc; i++)
        {
                if (c_strcasestr(argv[i], quitOnStop, maxLen) ||
                    c_strcasestr(argv[i], quitOnStop2, maxLen))
                {
                        ui->quitAfterStopping = true;
                        idx = i;
                }
        }
        if (idx >= 0)
                removeArgElement(argv, idx, argc);

        idx = -1;
        for (int i = 0; i < *argc; i++)
        {
                if (c_strcasestr(argv[i], exactOption, maxLen) ||
                    c_strcasestr(argv[i], exactOption2, maxLen))
                {
                        *exactSearch = true;
                        idx = i;
                }
        }
        if (idx >= 0)
                removeArgElement(argv, idx, argc);
}

// Returns 1 if the process is running
int isProcessRunning(pid_t pid)
{
        if (pid <= 0)
        {
                return 0; // Invalid PID
        }

        // Send signal 0 to check if the process exists
        if (kill(pid, 0) == 0)
        {
                return 1; // Process exists
        }

        // Check errno for detailed status
        if (errno == ESRCH)
        {
                return 0; // No such process
        }
        else if (errno == EPERM)
        {
                return 1; // Process exists but we don't have permission
        }

        return 0; // Other errors
}

const char *getTempDir()
{
        const char *tmpdir = getenv("TMPDIR");
        if (tmpdir != NULL)
        {
                return tmpdir; // Use TMPDIR if set (common on Android/Termux)
        }

        tmpdir = getenv("TEMP");
        if (tmpdir != NULL)
        {
                return tmpdir;
        }

        // Fallback to /tmp on Unix-like systems
        return "/tmp";
}

int isKewProcess(pid_t pid)
{
        char comm_path[64];
        char process_name[256];
        FILE *file;

        // First check /proc/[pid]/comm for the process name
        snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", pid);
        file = fopen(comm_path, "r");
        if (file != NULL)
        {
                if (fgets(process_name, sizeof(process_name), file))
                {
                        fclose(file);
                        // Remove trailing newline
                        process_name[strcspn(process_name, "\n")] = 0;

                        // Check if it's kew (process name might be truncated to
                        // 15 chars)
                        if (strstr(process_name, "kew") != NULL)
                        {
                                return 1; // It's likely kew
                        }
                }
                else
                {
                        fclose(file);
                }
        }

        return 0; // Not kew or couldn't determine
}

// Ensures only a single instance of kew can run at a time for the current user.
void exitIfAlreadyRunning()
{
        char pidfile_path[512]; // Increased size for longer paths
        const char *temp_dir = getTempDir();

        snprintf(pidfile_path, sizeof(pidfile_path), "%s/kew_%d.pid", temp_dir,
                 getuid());

        FILE *pidfile;
        pid_t pid;

        pidfile = fopen(pidfile_path, "r");
        if (pidfile != NULL)
        {
                if (fscanf(pidfile, "%d", &pid) == 1)
                {
                        fclose(pidfile);

#ifdef __ANDROID__
                        if (isProcessRunning(pid) && isKewProcess(pid))
#else
                        if (isProcessRunning(pid))
#endif
                        {
                                fprintf(
                                    stderr,
                                    "An instance of kew is already running. "
                                    "Pid: %d. Type 'kill %d' to remove it.\n",
                                    pid, pid);
                                exit(1);
                        }
                        else
                        {
                                unlink(pidfile_path);
                        }
                }
                else
                {
                        fclose(pidfile);
                        unlink(pidfile_path);
                }
        }

        // Create a new PID file
        pidfile = fopen(pidfile_path, "w");
        if (pidfile == NULL)
        {
                perror("Unable to create PID file");
                exit(1);
        }
        fprintf(pidfile, "%d\n", getpid());
        fclose(pidfile);
}

int directoryExists(const char *path)
{
        DIR *dir = opendir(path);
        if (dir != NULL)
        {
                closedir(dir);
                return 1;
        }

        return 0;
}

void clearInputBuffer()
{
        int c;
        while ((c = getchar()) != '\n' && c != EOF)
                ;
}

void setMusicPath()
{
        struct passwd *pw = getpwuid(getuid());
        char *user = NULL;

        clearScreen();

        if (pw)
        {
                user = pw->pw_name;
        }
        else
        {
                printf("Error: Could not retrieve user information.\n");
                printf("Please set a path to your music library.\n");
                printf("To set it, type: kew path \"/path/to/Music\".\n");
                exit(1);
        }

        // Music folder names in different languages
        const char *musicFolderNames[] = {
            "Music", "Música", "Musique", "Musik", "Musica", "Muziek",
            "Музыка", "音乐", "音楽", "음악", "موسيقى", "संगीत",
            "Müzik", "Musikk", "Μουσική", "Muzyka", "Hudba", "Musiikki",
            "Zene", "Muzică", "เพลง", "מוזיקה"};

        char path[PATH_MAX];
        int found = 0;
        int result = -1;
        char choice[2];

        AppSettings *settings = getAppSettings();

        for (size_t i = 0;
             i < sizeof(musicFolderNames) / sizeof(musicFolderNames[0]); i++)
        {
#ifdef __APPLE__
                snprintf(path, sizeof(path), "/Users/%s/%s", user,
                         musicFolderNames[i]);
#else
                snprintf(path, sizeof(path), "/home/%s/%s", user,
                         musicFolderNames[i]);
#endif

                if (directoryExists(path))
                {
                        found = 1;
                        printf("Do you want to use %s as your music library "
                               "folder?\n",
                               path);
                        printf("y = Yes\nn = Enter a path\n");

                        result = scanf("%1s", choice);

                        if (choice[0] == 'y' || choice[0] == 'Y')
                        {
                                c_strcpy(settings->path, path,
                                         sizeof(settings->path));
                                return;
                        }
                        else if (choice[0] == 'n' || choice[0] == 'N')
                        {
                                break;
                        }
                        else
                        {
                                choice[0] = 'n';
                                break;
                        }
                }
        }

        if (!found || (found && (choice[0] == 'n' || choice[0] == 'N')))
        {
                printf("Please enter the path to your music library "
                       "(/path/to/Music):\n");

                clearInputBuffer();

                if (fgets(path, sizeof(path), stdin) == NULL)
                {
                        printf("Error reading input.\n");
                        exit(1);
                }

                path[strcspn(path, "\n")] = '\0';

                if (directoryExists(path))
                {
                        c_strcpy(settings->path, path, sizeof(settings->path));
                }
                else
                {
                        printf("The entered path does not exist or is "
                               "inaccessible.\n");
                        exit(1);
                }
        }

        if (result == -1)
                exit(1);
}

void enableMouse(UISettings *ui)
{
        if (ui->mouseEnabled)
                enableTerminalMouseButtons();
}

void setTrackTitleAsWindowTitle(UISettings *ui)
{
        if (ui->trackTitleAsWindowTitle)
        {
                saveTerminalWindowTitle();
                setTerminalWindowTitle("kew");
        }
}

void initState(AppState *state)
{
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
        state->uiState.doNotifyMPRISSwitched = false;
        state->uiState.doNotifyMPRISPlaying = false;
        state->uiState.collapseView = false;
        state->uiState.waitingForNext = false;
        state->uiState.waitingForPlaylist = false;
        state->uiState.refresh = true;
        state->uiState.loadedNextSong = false;
        state->uiState.isFastForwarding = false;
        state->uiState.isRewinding = false;
        state->uiState.songWasRemoved = false;
        state->uiState.startFromTop = false;
        state->uiState.lastNotifiedId = -1;
        state->uiState.noPlaylist = false;
        state->uiState.logFile = NULL;
        state->tmpCache = NULL;
        state->uiSettings.LAST_ROW = " [F2 Playlist|F3 Library|F4 Track|F5 Search|F6 Help]";
        state->uiSettings.defaultColor = 150;
        state->uiSettings.defaultColorRGB.r = state->uiSettings.defaultColor;
        state->uiSettings.defaultColorRGB.g = state->uiSettings.defaultColor;
        state->uiSettings.defaultColorRGB.b = state->uiSettings.defaultColor;

        pthread_mutex_init(&(state->dataSourceMutex), NULL);
        pthread_mutex_init(&(state->switchMutex), NULL);

        setUnshuffledPlaylist(NULL);
        setFavoritesPlaylist(NULL);

        statePtr = state;
}

void initSettings(AppState *state, AppSettings *settings)
{
        getConfig(settings, &(state->uiSettings));

        UserData *userData = getUserData();

        userData->replayGainCheckFirst =
            state->uiSettings.replayGainCheckFirst;

        initKeyMappings(state, settings);
        enableMouse(&(state->uiSettings));
        setTrackTitleAsWindowTitle(&(state->uiSettings));
}

void initTheme(int argc, char *argv[], AppState *state)
{
        UISettings *ui = &(state->uiSettings);
        AppSettings *settings = getAppSettings();
        bool themeLoaded = false;

        // Command-line theme handling
        if (argc > 3 && strcmp(argv[1], "theme") == 0)
        {
                setErrorMessage("Couldn't load theme. Theme file names shouldn't contain space.");
        }
        else if (argc == 3 && strcmp(argv[1], "theme") == 0)
        {
                // Try to load the user-specified theme
                if (loadTheme(state, settings, argv[2], false) > 0)
                {
                        ui->colorMode = COLOR_MODE_THEME;
                        initDefaultState(state);
                        themeLoaded = true;
                }
                else
                {
                        // Failed to load user theme → fall back to
                        // default/ANSI
                        if (ui->colorMode == COLOR_MODE_THEME)
                        {
                                ui->colorMode = COLOR_MODE_DEFAULT;
                        }
                }
        }
        else if (ui->colorMode == COLOR_MODE_THEME)
        {
                // If UI has a themeName stored, try to load it
                if (loadTheme(state, settings, ui->themeName, false) > 0)
                {
                        ui->colorMode = COLOR_MODE_THEME;
                        themeLoaded = true;
                }
        }

        // If still in default mode, load default ANSI theme
        if (ui->colorMode == COLOR_MODE_DEFAULT)
        {
                // Load "default" ANSI theme, but don't overwrite
                // settings->theme
                if (loadTheme(state, settings, "default", true))
                {
                        themeLoaded = true;
                }
        }

        if (!themeLoaded && ui->colorMode != COLOR_MODE_ALBUM)
        {
                setErrorMessage("Couldn't load theme. Forgot to run 'sudo make install'?");
                ui->colorMode = COLOR_MODE_ALBUM;
        }
}

int main(int argc, char *argv[])
{
        AppState *state = getAppState();

        initState(state);
        exitIfAlreadyRunning();

        UISettings *ui = &(state->uiSettings);
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
                printAbout(NULL, ui);
                exit(0);
        }

        initSettings(state, settings);

        if (argc == 3 && (strcmp(argv[1], "path") == 0))
        {
                c_strcpy(settings->path, argv[2], sizeof(settings->path));
                setConfig(settings, ui);
                exit(0);
        }

        enterAlternateScreenBuffer();
        atexit(cleanupOnExit);

        if (settings->path[0] == '\0')
        {
                setMusicPath();
        }

        bool exactSearch = false;
        handleOptions(&argc, argv, ui, &exactSearch);
        loadFavoritesPlaylist(settings->path, &favoritesPlaylist);

        ensureDefaultThemes();
        initTheme(argc, argv, state);

        if (argc == 1)
        {
                initDefaultState(state);
        }
        else if (argc == 2 && strcmp(argv[1], "all") == 0)
        {
                playAll(state);
        }
        else if (argc == 2 && strcmp(argv[1], "albums") == 0)
        {
                playAllAlbums(state);
        }
        else if (argc == 2 && strcmp(argv[1], ".") == 0 && favoritesPlaylist->count != 0)
        {
                playFavoritesPlaylist(state);
        }
        else if (argc >= 2)
        {
                init(state);
                makePlaylist(&playlist, argc, argv, exactSearch, settings->path);

                if (playlist->count == 0)
                {
                        state->uiState.noPlaylist = true;
                        exit(0);
                }

                FileSystemEntry *library = getLibrary();

                markListAsEnqueued(library, playlist);
                run(state, true);
        }

        return 0;
}
