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

#include "common/appstate.h"
#include "common/common.h"
#include "common/events.h"

#include "sys/mpris.h"
#include "sys/notifications.h"

#include "ops/input.h"
#include "ops/library_ops.h"
#include "ops/playback_clock.h"
#include "ops/playback_ops.h"
#include "ops/playback_state.h"
#include "ops/playback_system.h"
#include "ops/playlist_ops.h"
#include "ops/settings.h"
#include "ops/trackmanager.h"

#include "ui/common_ui.h"
#include "ui/control_ui.h"
#include "ui/player_ui.h"
#include "ui/search_ui.h"
#include "ui/visuals.h"

#include "sys/systemintegration.h"

#include "utils/cache.h"
#include "utils/file.h"
#include "utils/term.h"
#include "utils/utils.h"

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

const char VERSION[] = "3.6.0";

AppState *statePtr = NULL;

void setEndOfListReached(void)
{
        AppState *state = getAppState();
        AudioData *audioData = getAudioData();
        PlaybackState *ps = getPlaybackState();

        ps->loadedNextSong = false;
        ps->waitingForNext = true;
        audioData->endOfListReached = true;
        audioData->currentFileIndex = 0;
        audioData->restart = true;
        ps->usingSongDataA = false;
        ps->loadingdata.loadA = true;

        clearCurrentSong();

        playbackCleanup();

        triggerRefresh();

        if (playbackIsRepeatListEnabled())
                repeatList();
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

void notifySongSwitch(SongData *currentSongData)
{
        AppState *state = getAppState();
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

void determineSongAndNotify(void)
{
        AppState *state = getAppState();
        SongData *currentSongData = NULL;

        bool isDeleted = determineCurrentSongData(&currentSongData);

        Node *current = getCurrentSong();

        if (currentSongData && current)
                current->song.duration = currentSongData->duration;

        if (state->uiState.lastNotifiedId != current->id)
        {
                if (!isDeleted)
                        notifySongSwitch(currentSongData);
        }
}

// Refreshes the player visually if conditions are met
void refreshPlayer()
{
        AppState *state = getAppState();
        PlaybackState *ps = getPlaybackState();
        AppSettings *settings = getAppSettings();

        int mutexResult = pthread_mutex_trylock(&(state->switchMutex));

        if (mutexResult != 0)
        {
                fprintf(stderr, "Failed to lock switch mutex.\n");
                return;
        }

        if (ps->notifyPlaying)
        {
                ps->notifyPlaying = false;

                emitStringPropertyChanged("PlaybackStatus", "Playing");
        }

        if (ps->notifySwitch)
        {
                ps->notifySwitch = false;

                notifyMPRISSwitch(getCurrentSongData());
        }

        if (shouldRefreshPlayer())
        {
                printPlayer(getCurrentSongData(), getElapsedSeconds(), settings);
        }

        pthread_mutex_unlock(&(state->switchMutex));
}

void resetListAfterDequeuingPlayingSong(void)
{
        AppState *state = getAppState();
        PlaybackState *ps = getPlaybackState();

        state->uiState.startFromTop = true;

        if (ps->lastPlayedId < 0)
                return;

        Node *node = findSelectedEntryById(getPlaylist(), ps->lastPlayedId);

        if (getCurrentSong() == NULL && node == NULL)
        {
                playbackStop();

                AudioData *audioData = getAudioData();

                ps->loadedNextSong = false;
                audioData->endOfListReached = true;
                audioData->restart = true;

                emitMetadataChanged("", "", "", "",
                                    "/org/mpris/MediaPlayer2/TrackList/NoTrack",
                                    NULL, 0);
                emitPlaybackStoppedMpris();

                playbackCleanup();

                triggerRefresh();

                playbackSwitchDecoder();

                unloadSongA();
                unloadSongB();
                state->uiState.songWasRemoved = true;

                UserData *userData = playbackGetUserData();

                userData->currentSongData = NULL;

                audioData->currentFileIndex = 0;
                audioData->restart = true;
                ps->waitingForNext = true;

                PlaybackState *ps = getPlaybackState();

                ps->loadingdata.loadA = true;
                ps->usingSongDataA = false;

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

FileSystemEntry *enqueue(FileSystemEntry *entry)
{
        AppState *state = getAppState();
        FileSystemEntry *firstEnqueuedEntry = NULL;
        PlaybackState *ps = getPlaybackState();
        AudioData *audioData = getAudioData();

        if (audioData->restart)
        {
                Node *lastSong = findSelectedEntryById(getPlaylist(), ps->lastPlayedId);
                state->uiState.startFromTop = false;

                if (lastSong == NULL)
                {
                        if (getPlaylist()->tail != NULL)
                                ps->lastPlayedId = getPlaylist()->tail->id;
                        else
                        {
                                ps->lastPlayedId = -1;
                                state->uiState.startFromTop = true;
                        }
                }
        }

        pthread_mutex_lock(&(getPlaylist()->mutex));

        FileSystemEntry *chosenDir = getChosenDir();

        firstEnqueuedEntry = enqueueSongs(entry, &chosenDir);

        setChosenDir(chosenDir);

        resetListAfterDequeuingPlayingSong();

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

void playPostProcessing(bool wasEndOfList)
{
        AppState *state = getAppState();
        PlaybackState *ps = getPlaybackState();

        if ((state->uiState.songWasRemoved && getCurrentSong() != NULL))
        {
                state->uiState.songWasRemoved = false;
        }

        if (wasEndOfList)
        {
                ps->skipOutOfOrder = false;
        }

        AudioData *audioData = getAudioData();

        audioData->endOfListReached = false;
}

FileSystemEntry *libraryEnqueue(PlayList *playlist)
{
        AppState *state = getAppState();
        FileSystemEntry *entry = state->uiState.currentLibEntry;
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

                autostartIfStopped(firstEnqueuedEntry);

                markListAsEnqueued(getLibrary(), playlist);

                PlayList *unshuffledPlaylist = getUnshuffledPlaylist();

                deepCopyPlayListOntoList(playlist, &unshuffledPlaylist);
                shufflePlaylist(playlist);
                setUnshuffledPlaylist(unshuffledPlaylist);
        }
        else
        {
                firstEnqueuedEntry = enqueue(entry); // Enqueue song
        }

        return firstEnqueuedEntry;
}

FileSystemEntry *searchEnqueue(PlayList *playlist)
{
        PlaybackState *ps = getPlaybackState();

        pthread_mutex_lock(&(playlist->mutex));

        FileSystemEntry *entry = getCurrentSearchEntry();

        ps->waitingForPlaylist = false;

        setChosenDir(entry);

        FileSystemEntry *chosenDir = getChosenDir();

        FileSystemEntry *firstEnqueuedEntry = enqueueSongs(entry, &chosenDir);

        setChosenDir(chosenDir);

        resetListAfterDequeuingPlayingSong();

        pthread_mutex_unlock(&(playlist->mutex));

        return firstEnqueuedEntry;
}

void clearAndPlay(Node *song)
{
        AudioData *audioData = getAudioData();
        PlaybackState *ps = getPlaybackState();

        playbackCleanup();

        ps->loadedNextSong = true;

        ps->nextSongNeedsRebuilding = false;

        unloadSongA();
        unloadSongB();

        ps->usingSongDataA = false;

        audioData->currentFileIndex = 0;

        ps->loadingdata.loadA = true;

        bool wasEndOfList = playPreProcessing();

        playbackPlay();

        if (play(song) < 0)
        {
                skipToSong(song->next->id, true);
        }

        playPostProcessing(wasEndOfList);

        ps->skipOutOfOrder = false;
        ps->usingSongDataA = true;
}

void playlistPlay(PlayList *playlist)
{
        AppState *state = getAppState();
        PlaybackState *ps = getPlaybackState();

        if (!isDigitsPressed())
        {
                Node *current = getCurrentSong();

                if (playbackIsPaused() && current != NULL &&
                    state->uiState.chosenNodeId == current->id)
                {
                        togglePause();
                }
                else
                {
                        Node *song = NULL;
                        findNodeInList(playlist,
                                       state->uiState.chosenNodeId,
                                       &song);

                        clearAndPlay(song);
                }
        }
        else
        {
                state->uiState.resetPlaylistDisplay = true;
                int songNumber = getSongNumber(getDigitsPressed());

                resetDigitsPressed();

                ps->nextSongNeedsRebuilding = false;

                skipToNumberedSong(songNumber);
        }
}

FileSystemEntry *viewEnqueue(PlayList *playlist)
{
        AppState *state = getAppState();
        FileSystemEntry *firstEnqueuedEntry = NULL;

        switch (state->currentView)
        {
        case LIBRARY_VIEW:
                firstEnqueuedEntry = libraryEnqueue(playlist);
                break;
        case SEARCH_VIEW:
                firstEnqueuedEntry = searchEnqueue(playlist);
                break;
        case PLAYLIST_VIEW:
                playlistPlay(playlist);
                break;
        default:
                return NULL;
        }

        return firstEnqueuedEntry;
}

void enqueueHandler(bool playImmediately)
{
        PlayList *playlist = getPlaylist();
        Node *current = getCurrentSong();
        bool wasEmpty = (playlist->count == 0);
        bool canGoNextBefore = (current != NULL && current->next != NULL);

        FileSystemEntry *firstEnqueuedEntry = viewEnqueue(playlist);

        if (playImmediately && firstEnqueuedEntry && !wasEmpty)
        {
                Node *song = findPathInPlaylist(firstEnqueuedEntry->fullPath, playlist);
                clearAndPlay(song);
        }

        Node *currentAfter = getCurrentSong();
        bool canGoNextAfter = (currentAfter != NULL && currentAfter->next != NULL);

        if (canGoNextBefore != canGoNextAfter)
        {
                emitBooleanPropertyChanged("CanGoNext", canGoNextAfter);
        }
}

void handleGoToSong(void)
{
        AppState *state = getAppState();
        Node *currentSong = getCurrentSong();
        PlayList *playlist = getPlaylist();
        PlayList *unshuffledPlaylist = getUnshuffledPlaylist();
        FileSystemEntry *library = getLibrary();
        PlaybackState *ps = getPlaybackState();
        AudioData *audioData = getAudioData();

        bool canGoNext = (currentSong != NULL && currentSong->next != NULL);

        if (state->currentView == LIBRARY_VIEW)
        {
                FileSystemEntry *entry = state->uiState.currentLibEntry;

                if (entry == NULL)
                        return;

                // Enqueue playlist
                if (pathEndsWith(entry->fullPath, "m3u") ||
                    pathEndsWith(entry->fullPath, "m3u8"))
                {
                        FileSystemEntry *firstEnqueuedEntry = NULL;

                        Node *prevTail = playlist->tail;

                        if (playlist != NULL)
                        {
                                readM3UFile(entry->fullPath, playlist, library);

                                if (prevTail != NULL && prevTail->next != NULL)
                                {
                                        firstEnqueuedEntry = findCorrespondingEntry(
                                            library, prevTail->next->song.filePath);
                                }
                                else if (playlist->head != NULL)
                                {
                                        firstEnqueuedEntry = findCorrespondingEntry(
                                            library, playlist->head->song.filePath);
                                }

                                autostartIfStopped(firstEnqueuedEntry);

                                markListAsEnqueued(library, playlist);

                                deepCopyPlayListOntoList(playlist, &unshuffledPlaylist);
                        }
                }
                else
                        enqueue(entry); // Enqueue song
        }
        else if (state->currentView == SEARCH_VIEW)
        {
                pthread_mutex_lock(&(playlist->mutex));

                FileSystemEntry *entry = getCurrentSearchEntry();

                setChosenDir(entry);

                FileSystemEntry *chosen = getChosenDir();

                enqueueSongs(entry, &chosen);

                resetListAfterDequeuingPlayingSong();

                pthread_mutex_unlock(&(playlist->mutex));
        }
        else if (state->currentView == PLAYLIST_VIEW)
        {
                if (getDigitsPressed() == 0)
                {
                        if (playbackIsPaused() && currentSong != NULL &&
                            state->uiState.chosenNodeId == currentSong->id)
                        {
                                togglePause();
                        }
                        else
                        {
                                playbackSafeCleanup();

                                ps->loadedNextSong = true;

                                ps->nextSongNeedsRebuilding = false;

                                unloadSongA();
                                unloadSongB();

                                ps->usingSongDataA = false;
                                audioData->currentFileIndex = 0;
                                ps->loadingdata.loadA = true;

                                bool wasEndOfList = playPreProcessing();

                                playbackPlay();

                                Node *found = NULL;
                                findNodeInList(playlist,
                                               state->uiState.chosenNodeId,
                                               &found);
                                play(found);

                                playPostProcessing(wasEndOfList);

                                ps->skipOutOfOrder = false;
                                ps->usingSongDataA = true;
                        }
                }
                else
                {
                        state->uiState.resetPlaylistDisplay = true;
                        int songNumber = getSongNumber(getDigitsPressed());
                        resetDigitsPressed();

                        ps->nextSongNeedsRebuilding = false;

                        skipToNumberedSong(songNumber);
                }
        }

        // Handle MPRIS CanGoNext
        bool couldGoNext = (currentSong != NULL && currentSong->next != NULL);
        if (canGoNext != couldGoNext)
        {
                emitBooleanPropertyChanged("CanGoNext", couldGoNext);
        }
}

void enqueueAndPlay(void)
{
        AppState *state = getAppState();
        FileSystemEntry *firstEnqueuedEntry = NULL;
        PlayList *playlist = getPlaylist();
        PlaybackState *ps = getPlaybackState();
        AudioData *audioData = getAudioData();

        bool wasEmpty = (playlist->count == 0);

        bool wasEndOfList = playPreProcessing();

        if (state->currentView == PLAYLIST_VIEW)
        {
                handleGoToSong();
                return;
        }

        if (state->currentView == LIBRARY_VIEW)
        {
                firstEnqueuedEntry = enqueue(state->uiState.currentLibEntry);
        }

        if (state->currentView == SEARCH_VIEW)
        {
                FileSystemEntry *entry = getCurrentSearchEntry();
                firstEnqueuedEntry = enqueue(entry);

                setChosenDir(entry);
        }

        if (firstEnqueuedEntry && !wasEmpty)
        {
                Node *song =
                    findPathInPlaylist(firstEnqueuedEntry->fullPath, playlist);

                ps->loadedNextSong = true;

                ps->nextSongNeedsRebuilding = false;

                playbackSafeCleanup();

                unloadSongA();
                unloadSongB();

                ps->usingSongDataA = false;
                audioData->currentFileIndex = 0;
                ps->loadingdata.loadA = true;

                playbackPlay();

                play(song);

                playPostProcessing(wasEndOfList);

                ps->skipOutOfOrder = false;
                ps->usingSongDataA = true;
        }
}

void gotoBeginningOfPlaylist(void)
{
        pressDigit(1);
        enqueueHandler(false);
}

void gotoEndOfPlaylist()
{
        if (isDigitsPressed())
        {
                enqueueHandler(false);
        }
        else
        {
                skipToLastSong();
        }
}

void handleInput(void)
{
        struct Event event = processInput();

        AppState *state = getAppState();
        AppSettings *settings = getAppSettings();
        PlayList *playlist = getPlaylist();
        int chosenRow = 0;

        switch (event.type)
        {
        case EVENT_GOTOBEGINNINGOFPLAYLIST:
                gotoBeginningOfPlaylist();
                break;
        case EVENT_GOTOENDOFPLAYLIST:
                gotoEndOfPlaylist();
                break;
        case EVENT_ENQUEUE:
                enqueueHandler(false);
                break;
        case EVENT_ENQUEUEANDPLAY:
                enqueueHandler(true);
                break;
        case EVENT_PLAY_PAUSE:
                if (playbackIsStopped())
                {
                        enqueueHandler(false);
                }
                else
                {
                        togglePause();
                }
                break;
        case EVENT_TOGGLEVISUALIZER:
                toggleVisualizer(settings, &(state->uiSettings));
                break;
        case EVENT_TOGGLEREPEAT:
                toggleRepeat();
                break;
        case EVENT_TOGGLEASCII:
                toggleAscii(settings, &(state->uiSettings));
                break;
        case EVENT_SHUFFLE:
                toggleShuffle();
                emitShuffleChanged();
                break;
        case EVENT_SHOWLYRICSPAGE:
                toggleShowLyricsPage();
                break;
        case EVENT_CYCLECOLORMODE:
                cycleColorMode();
                break;
        case EVENT_CYCLETHEMES:
                cycleThemes(settings);
                break;
        case EVENT_TOGGLENOTIFICATIONS:
                toggleNotifications(&(state->uiSettings), settings);
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
                playbackVolumeChange(5);
                emitVolumeChanged();
                break;
        case EVENT_VOLUME_DOWN:
                playbackVolumeChange(-5);
                emitVolumeChanged();
                break;
        case EVENT_NEXT:
                skipToNextSong();
                break;
        case EVENT_PREV:
                skipToPrevSong();
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

Node *determineNextSong(PlayList *playlist)
{
        AppState *state = getAppState();
        Node *current = NULL;
        PlaybackState *ps = getPlaybackState();

        if (ps->waitingForPlaylist)
        {
                return playlist->head;
        }
        else if (ps->waitingForNext)
        {
                Node *songToStartFrom = getSongToStartFrom();

                if (songToStartFrom != NULL)
                {
                        findNodeInList(playlist, songToStartFrom->id, &current);
                        return current ? current : NULL;
                }
                else if (ps->lastPlayedId >= 0)
                {
                        current = findSelectedEntryById(playlist, ps->lastPlayedId);
                        if (current != NULL && current->next != NULL)
                                current = current->next;

                        return current;
                }

                // fallback if nothing else
                if (state->uiState.startFromTop)
                {
                        state->uiState.startFromTop = false;
                        return playlist->head;
                }
                else
                {
                        return playlist->tail;
                }
        }

        return NULL;
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
                playbackResumePlayback();
        }

        triggerRefresh();
        resetClock();

        return res;
}

void updateNextSongIfNeeded(void)
{
        loadNextSong();
        determineSongAndNotify();
}

void checkAndLoadNextSong(void)
{
        AppState *state = getAppState();
        AudioData *audioData = getAudioData();
        PlaybackState *ps = getPlaybackState();

        if (audioData->restart)
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

                        // Reset relevant UI state
                        audioData->restart = false;
                        ps->waitingForPlaylist = false;
                        ps->waitingForNext = false;
                        state->uiState.songWasRemoved = false;

                        if (playbackIsShuffleEnabled())
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

void handleSkipOutOfOrder(void)
{
        PlaybackState *ps = getPlaybackState();

        if (!ps->skipOutOfOrder && !playbackIsRepeatEnabled())
        {
                setCurrentSongToNext();
        }
        else
        {
                ps->skipOutOfOrder = false;
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

        if (!playbackIsRepeatEnabled() || current == NULL)
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

void updatePlayerStatus(void)
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

                if (playbackisDone())
                {
                        prepareNextSong();
                        playbackSwitchDecoder();
                }
        }
        else
        {
                playbackSetEofHandled();
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

                updatePlayerStatus();
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

void initFirstPlay(Node *song)
{
        updateLastInputTime();
        resetClock();

        UserData *userData = playbackGetUserData();
        PlaybackState *ps = getPlaybackState();

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

                PlaybackState *ps = getPlaybackState();
                ps->loadingdata.loadA = true;

                res = loadFirst(song);

                if (res >= 0)
                {
                        res = playbackCreate();
                }
                if (res >= 0)
                {
                        playbackResumePlayback();
                }

                if (res < 0)
                        setEndOfListReached();
        }

        if (song == NULL || res < 0)
        {
                song = NULL;
                if (!ps->waitingForNext)
                        ps->waitingForPlaylist = true;
        }

        ps->loadedNextSong = false;
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
        PlaybackState *ps = getPlaybackState();

        pthread_mutex_lock(&(state->dataSourceMutex));

        playbackFreeDecoders();

        playbackShutdown();

        emitPlaybackStoppedMpris();

        bool noMusicFound = false;

        FileSystemEntry *library = getLibrary();

        if (library == NULL || library->children == NULL)
        {
                noMusicFound = true;
        }

        playbackUnloadSongs(playbackGetUserData());

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
        freeMainDirectoryTree();
        freePlaylists();
        setDefaultTextColor();
        pthread_mutex_destroy(&(ps->loadingdata.mutex));
        pthread_mutex_destroy(&(playlist->mutex));
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

void run(bool startPlaying)
{
        AppState *state = getAppState();
        PlayList *playlist = getPlaylist();
        PlayList *unshuffledPlaylist = getUnshuffledPlaylist();
        PlaybackState *ps = getPlaybackState();

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

        if (startPlaying)
                setCurrentSong(playlist->head);
        else if (playlist->count > 0)
                ps->waitingForNext = true;

        initFirstPlay(getCurrentSong());
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

void initResize(void)
{
        signal(SIGWINCH, handleResize);

        struct sigaction sa;
        sa.sa_handler = resetResizeFlag;
        sigemptyset(&(sa.sa_mask));
        sa.sa_flags = 0;
        sigaction(SIGALRM, &sa, NULL);
}

void init(void)
{
        AppState *state = getAppState();
        disableTerminalLineInput();
        setRawInputMode();
        initResize();
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &state->uiState.windowSize);
        enableScrolling();
        setNonblockingMode();
        state->tmpCache = createCache();

        PlaybackState *ps = getPlaybackState();
        UserData *userData = playbackGetUserData();
        AppSettings *settings = getAppSettings();
        PlayList *playlist = getPlaylist();
        AudioData *audioData = getAudioData();

        c_strcpy(ps->loadingdata.filePath, "", sizeof(ps->loadingdata.filePath));
        ps->loadingdata.songdataA = NULL;
        ps->loadingdata.songdataB = NULL;
        ps->loadingdata.loadA = true;
        ps->loadingdata.loadingFirstDecoder = true;
        audioData->restart = true;
        userData->songdataADeleted = true;
        userData->songdataBDeleted = true;
        unsigned int seed = (unsigned int)time(NULL);
        srand(seed);
        pthread_mutex_init(&(ps->loadingdata.mutex), NULL);
        pthread_mutex_init(&(playlist->mutex), NULL);

        freeSearchResults();
        resetChosenDir();
        createLibrary(settings, getLibraryFilePath());
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

        AudioData *audioData = getAudioData();

        audioData->restart = true;
        audioData->endOfListReached = true;
        ps->loadedNextSong = false;

        state->currentView = LIBRARY_VIEW;

        run(false);
}

void playFavoritesPlaylist(void)
{
        PlayList *playlist = getPlaylist();
        PlayList *favoritesPlaylist = getFavoritesPlaylist();

        if (favoritesPlaylist->count == 0)
        {
                printf("Couldn't find any songs in the special playlist. Add a "
                       "song by pressing '.' while it's playing. \n");
                exit(0);
        }

        init();

        FileSystemEntry *library = getLibrary();

        deepCopyPlayListOntoList(favoritesPlaylist, &playlist);
        shufflePlaylist(playlist);
        setPlaylist(playlist);
        markListAsEnqueued(library, playlist);

        run(true);
}

void playAll(void)
{
        init();

        FileSystemEntry *library = getLibrary();
        PlayList *playlist = getPlaylist();

        createPlayListFromFileSystemEntry(library, playlist, MAX_FILES);

        if (playlist->count == 0)
        {
                exit(0);
        }

        shufflePlaylist(playlist);
        markListAsEnqueued(library, playlist);
        run(true);
}

void playAllAlbums(void)
{
        init();

        PlayList *playlist = getPlaylist();
        FileSystemEntry *library = getLibrary();
        addShuffledAlbumsToPlayList(library, playlist, MAX_FILES);

        if (playlist->count == 0)
        {
                exit(0);
        }

        markListAsEnqueued(library, playlist);
        run(true);
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
        char expanded[MAXPATHLEN];

        expandPath(path, expanded);

        DIR *dir = opendir(expanded);

        if (dir)
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

void setMusicPath(UISettings *ui)
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
                printf("Please set a path to your music library.\n");
                printf("To set it, type: kew path \"~/Music\".\n");
                exit(1);
        }

        ui->color.r = ui->kewColorRGB.r;
        ui->color.g = ui->kewColorRGB.g;
        ui->color.b = ui->kewColorRGB.b;

        printf("\n\n\n\n");

        printLogoArt(ui, 0, true, true, false);

        printf("\n\n\n\n");

        applyColor(COLOR_MODE_ALBUM, ui->theme.text, ui->defaultColorRGB);

        int indent = calcIndentNormal() + 1;

        // Music folder names in different languages
        const char *musicFolderNames[] = {
            "Music", "Música", "Musique", "Musik", "Musica", "Muziek",
            "Музыка", "音乐", "音楽", "음악", "موسيقى", "संगीत",
            "Müzik", "Musikk", "Μουσική", "Muzyka", "Hudba", "Musiikki",
            "Zene", "Muzică", "เพลง", "מוזיקה"};

        char path[MAXPATHLEN];
        int found = -1;
        char choice[MAXPATHLEN];

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

                        printf("\n\n");

                        printBlankSpaces(indent);

                        printf("Music Library: ");

                        applyColor(COLOR_MODE_ALBUM, ui->theme.text, ui->kewColorRGB);

                        printf("%s\n\n", path);

                        applyColor(COLOR_MODE_ALBUM, ui->theme.text, ui->defaultColorRGB);

                        printBlankSpaces(indent);

                        printf("Is this correct? Press Enter.\n\n");

                        printBlankSpaces(indent);

                        printf("Or type a path (no quotes or single-quotes):\n\n");

                        printBlankSpaces(indent);

                        applyColor(COLOR_MODE_ALBUM, ui->theme.text, ui->kewColorRGB);

                        if (fgets(choice, sizeof(choice), stdin) == NULL)
                        {
                                printBlankSpaces(indent);
                                printf("Error reading input.\n");
                                exit(1);
                        }

                        if (choice[0] == '\n' || choice[0] == '\0')
                        {
                                c_strcpy(settings->path, path, sizeof(settings->path));
                                printBlankSpaces(indent);
                                return;
                        }
                        else
                        {
                                break;
                        }
                }
        }

        printBlankSpaces(indent);

        // No standard music folder was found
        if (found < 1)
        {
                printf("Type a path (no quotes or single-quotes):\n\n");

                printBlankSpaces(indent);

                applyColor(COLOR_MODE_ALBUM, ui->theme.text, ui->kewColorRGB);

                if (fgets(choice, sizeof(choice), stdin) == NULL)
                {
                        printBlankSpaces(indent);
                        printf("Error reading input.\n");
                        exit(1);
                }
        }

        printBlankSpaces(indent);
        choice[strcspn(choice, "\n")] = '\0';

        // Set the path if the chosen directory exists
        if (directoryExists(choice))
        {
                char expanded[MAXPATHLEN];

                expandPath(choice, expanded);

                c_strcpy(settings->path, expanded, sizeof(settings->path));

                found = 1;
        }
        else
        {
                found = -1;
        }

        if (found == -1)
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

        statePtr = state;
}

void initSettings(AppSettings *settings)
{
        AppState *state = getAppState();
        UserData *userData = playbackGetUserData();

        getConfig(settings, &(state->uiSettings));

        userData->replayGainCheckFirst =
            state->uiSettings.replayGainCheckFirst;

        initKeyMappings(settings);
        enableMouse(&(state->uiSettings));
        setTrackTitleAsWindowTitle(&(state->uiSettings));
}

void initTheme(int argc, char *argv[])
{
        AppState *state = getAppState();
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
                if (loadTheme(settings, argv[2], false) > 0)
                {
                        ui->colorMode = COLOR_MODE_THEME;
                        initDefaultState();
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
                if (loadTheme(settings, ui->themeName, false) > 0)
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
                if (loadTheme(settings, "default", true))
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

        initState();
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
                printAbout(NULL);
                exit(0);
        }

        initSettings(settings);

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
                setMusicPath(ui);
        }

        bool exactSearch = false;
        handleOptions(&argc, argv, ui, &exactSearch);
        loadFavoritesPlaylist(settings->path, &favoritesPlaylist);

        ensureDefaultThemes();
        initTheme(argc, argv);

        if (argc == 1)
        {
                initDefaultState();
        }
        else if (argc == 2 && strcmp(argv[1], "all") == 0)
        {
                playAll();
        }
        else if (argc == 2 && strcmp(argv[1], "albums") == 0)
        {
                playAllAlbums();
        }
        else if (argc == 2 && strcmp(argv[1], ".") == 0 && favoritesPlaylist->count != 0)
        {
                playFavoritesPlaylist();
        }
        else if (argc >= 2)
        {
                init();
                makePlaylist(&playlist, argc, argv, exactSearch, settings->path);

                if (playlist->count == 0)
                {
                        state->uiState.noPlaylist = true;
                        exit(0);
                }

                FileSystemEntry *library = getLibrary();

                markListAsEnqueued(library, playlist);
                run(true);
        }

        return 0;
}
