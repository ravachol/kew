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
#include "common_ui.h"
#include "events.h"
#include "file.h"
#include "imgfunc.h"
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
#include <ctype.h>
#include <dirent.h>
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

#define MAX_TMP_SEQ_LEN 256 // Maximum length of temporary sequence buffer
#define COOLDOWN_MS 500
#define COOLDOWN2_MS 100

#define TMPPIDFILE "/tmp/kew_"

#ifndef PREFIX
#define PREFIX "/usr/local" // Fallback if not set in the makefile
#endif

FILE *logFile = NULL;
struct winsize windowSize;
char digitsPressed[MAX_SEQ_LEN];
int digitsPressedCount = 0;
bool startFromTop = false;
int lastNotifiedId = -1;
bool songWasRemoved = false;
bool noPlaylist = false;
GMainLoop *main_loop;
EventMapping keyMappings[NUM_KEY_MAPPINGS];
struct timespec lastInputTime;
bool exactSearch = false;
int fuzzySearchThreshold = 4;
int maxDigitsPressedCount = 9;
int isNewSearchTerm = false;
bool wasEndOfList = false;

void updateLastInputTime(void)
{
        clock_gettime(CLOCK_MONOTONIC, &lastInputTime);
}

bool isCooldownElapsed(int milliSeconds)
{
        struct timespec currentTime;
        clock_gettime(CLOCK_MONOTONIC, &currentTime);
        double elapsedMilliseconds =
            (currentTime.tv_sec - lastInputTime.tv_sec) * 1000.0 +
            (currentTime.tv_nsec - lastInputTime.tv_nsec) / 1000000.0;

        return elapsedMilliseconds >= milliSeconds;
}

enum EventType getMouseLastRowEvent(int mouseXOnLastRow)
{
        enum EventType result = EVENT_NONE;

        size_t lastRowLen = strlen(LAST_ROW);
        if (mouseXOnLastRow < 0 || (size_t)mouseXOnLastRow > lastRowLen)
        {
                // Out of bounds, return default
                return EVENT_NONE;
        }

        int viewClicked = 1;
        for (int i = 0; i < mouseXOnLastRow; i++)
        {
                if (LAST_ROW[i] == '|')
                {
                        viewClicked++;
                }
        }

        switch (viewClicked)
        {
        case 1:
                result = EVENT_SHOWPLAYLIST;
                break;
        case 2:
                result = EVENT_SHOWLIBRARY;
                break;
        case 3:
                result = EVENT_SHOWTRACK;
                break;
        case 4:
                result = EVENT_SHOWSEARCH;
                break;
        case 5:
                result = EVENT_SHOWKEYBINDINGS;
                break;
        default:
                result = EVENT_NONE;
                break;
        }

        // Switch to library view if track view is clicked and no song is
        // currently playing
        if (result == EVENT_SHOWTRACK && getCurrentSongData() == NULL)
        {
                result = EVENT_SHOWLIBRARY;
        }

        return result;
}

bool mouseInputHandled(char *seq, int i, struct Event *event)
{
        if (!seq || !event)
                return false;

        if (i < 0 || i >= NUM_KEY_MAPPINGS || keyMappings[i].seq == NULL)
                return false;

        const char *expected = keyMappings[i].seq;

        char tmpSeq[MAX_SEQ_LEN];
        size_t src_len = strnlen(seq, MAX_SEQ_LEN - 1);

        if (src_len < 4) // Must be at least ESC[ M + 3 digits
                return false;

        snprintf(tmpSeq, sizeof tmpSeq, "%.*s", (int)src_len, seq);

        int mouseButton = 0, mouseX = 0, mouseY = 0;
        const char *end = tmpSeq + src_len;
        const char *p = tmpSeq + 3; // Skip ESC[ M

        for (int field = 0; field < 3 && p && *p && p < end; ++field)
        {
                char *endptr;
                long val = strtol(p, &endptr, 10);
                if (endptr == p || endptr > end) // no progress or out of bounds
                        break;
                p = endptr;

                if (*p == ';')
                        ++p;

                switch (field)
                {
                case 0:
                        mouseButton = (int)val;
                        break;
                case 1:
                        mouseX = (int)val;
                        break;
                case 2:
                        mouseY = (int)val;
                        break;
                }
        }

        if (progressBarLength > 0)
        {
                long long deltaCol =
                    (long long)mouseX - (long long)progressBarCol;

                if (deltaCol >= 0 && deltaCol <= (long long)progressBarLength)
                {
                        double position =
                            (double)deltaCol / (double)progressBarLength;
                        double duration = getCurrentSongDuration();
                        draggedPositionSeconds = duration * position;
                }
                else
                {
                        draggedPositionSeconds = 0.0;
                }
        }
        else
        {
                draggedPositionSeconds = 0.0;
        }

        if (mouseY == lastRowRow && lastRowCol > 0 && mouseX - lastRowCol > 0 &&
            mouseX - lastRowCol < (int)strlen(LAST_ROW) &&
            mouseButton != MOUSE_DRAG)
        {
                event->type = getMouseLastRowEvent(mouseX - lastRowCol);
                return true;
        }

        if ((mouseY == progressBarRow || draggingProgressBar) &&
            mouseX - progressBarCol >= 0 &&
            mouseX - progressBarCol < progressBarLength &&
            appState.currentView == TRACK_VIEW)
        {
                if (mouseButton == MOUSE_DRAG || mouseButton == MOUSE_CLICK)
                {
                        draggingProgressBar = true;
                        gint64 newPosUs =
                            (gint64)(draggedPositionSeconds * G_USEC_PER_SEC);
                        setPosition(newPosUs);
                }
                return true;
        }

        size_t expected_len = strlen(expected);
        if (strlen(seq) < expected_len + 1)
                return false;

        if (strncmp(seq + 1, expected, expected_len) == 0)
        {
                event->type = keyMappings[i].eventType;
                return true;
        }

        return false;
}

struct Event processInput(UISettings *ui)
{
        struct Event event;
        event.type = EVENT_NONE;
        event.key[0] = '\0';
        bool cooldownElapsed = false;
        bool cooldown2Elapsed = false;

        if (isCooldownElapsed(COOLDOWN_MS))
                cooldownElapsed = true;

        if (isCooldownElapsed(COOLDOWN2_MS))
                cooldown2Elapsed = true;

        int seqLength = 0;
        char seq[MAX_SEQ_LEN];
        seq[0] = '\0'; // Set initial value
        int keyReleased = 0;

        bool foundInput = false;

        // Find input
        while (isInputAvailable())
        {
                char tmpSeq[MAX_TMP_SEQ_LEN];

                seqLength =
                    seqLength + readInputSequence(tmpSeq, sizeof(tmpSeq));

                // Release most keys directly, seekbackward and seekforward can
                // be read continuously
                if (seqLength <= 0 &&
                    strcmp(seq + 1, settings.seekBackward) != 0 &&
                    strcmp(seq + 1, settings.seekForward) != 0)
                {
                        keyReleased = 1;
                        break;
                }

                foundInput = true;

                size_t seq_len = strnlen(seq, MAX_SEQ_LEN);
                size_t remaining_space = MAX_SEQ_LEN - seq_len;

                if (remaining_space < 1)
                {
                        break;
                }

                snprintf(seq + seq_len, remaining_space, "%s", tmpSeq);

                // This slows the continous reads down to not get a a too fast
                // scrolling speed
                if (strcmp(seq + 1, settings.hardScrollUp) == 0 ||
                    strcmp(seq + 1, settings.hardScrollDown) == 0 ||
                    strcmp(seq + 1, settings.scrollUpAlt) == 0 ||
                    strcmp(seq + 1, settings.scrollDownAlt) == 0 ||
                    strcmp(seq + 1, settings.seekBackward) == 0 ||
                    strcmp(seq + 1, settings.seekForward) == 0 ||
                    strcmp(seq + 1, settings.nextPage) == 0 ||
                    strcmp(seq + 1, settings.prevPage) == 0)
                {
                        keyReleased = 0;
                        readInputSequence(
                            tmpSeq,
                            sizeof(tmpSeq)); // Dummy read to prevent scrolling
                                             // after key released
                        break;
                }

                keyReleased = 0;
        }

        if (!foundInput && cooldownElapsed)
        {
                if (!draggingProgressBar)
                        flushSeek();
                return event;
        }

        if (keyReleased)
                return event;

        event.type = EVENT_NONE;

        c_strcpy(event.key, seq, MAX_SEQ_LEN);

        if (appState.currentView == SEARCH_VIEW)
        {
                if (strcmp(event.key, "\x7F") == 0 ||
                    strcmp(event.key, "\x08") == 0)
                {
                        removeFromSearchText();
                        resetSearchResult();
                        fuzzySearch(getLibrary(), fuzzySearchThreshold);
                        event.type = EVENT_SEARCH;
                }
                else if (((strnlen(event.key, sizeof(event.key)) == 1 &&
                           event.key[0] != '\033' && event.key[0] != '\n' &&
                           event.key[0] != '\t' && event.key[0] != '\r') ||
                          strcmp(event.key, " ") == 0 ||
                          (unsigned char)event.key[0] >= 0xC0) &&
                         strcmp(event.key, "Z") != 0 &&
                         strcmp(event.key, "X") != 0 &&
                         strcmp(event.key, "C") != 0 &&
                         strcmp(event.key, "V") != 0 &&
                         strcmp(event.key, "B") != 0 &&
                         strcmp(event.key, "N") != 0)
                {
                        addToSearchText(event.key, ui);
                        resetSearchResult();
                        fuzzySearch(getLibrary(), fuzzySearchThreshold);
                        event.type = EVENT_SEARCH;
                }
        }

        if (seq[0] == 127)
        {
                seq[0] = '\b'; // Treat as Backspace
        }

        bool handledMouse = false;
        // Set event for pressed key
        for (int i = 0; i < NUM_KEY_MAPPINGS; i++)
        {
                if (keyMappings[i].seq[0] != '\0' &&
                    ((seq[0] == '\033' && strnlen(seq, MAX_SEQ_LEN) > 1 &&
                      strcmp(seq, "\033\n") != 0 &&
                      strcmp(seq + 1, keyMappings[i].seq) == 0) ||
                     strcmp(seq, keyMappings[i].seq) == 0))
                {
                        if (event.type == EVENT_SEARCH &&
                            keyMappings[i].eventType != EVENT_GOTOSONG)
                        {
                                break;
                        }

                        event.type = keyMappings[i].eventType;
                        break;
                }

                // Received mouse input instead of keyboard input
                if (keyMappings[i].seq[0] != '\0' &&
                    strncmp(seq, "\033[<", 3) == 0 &&
                    strnlen(seq, MAX_SEQ_LEN) > 4 && strchr(seq, 'M') != NULL &&
                    mouseInputHandled(seq, i, &event))
                {
                        handledMouse = true;
                        break;
                }
        }

        if (!handledMouse)
        {
                // Stop dragging progress bar
                draggingProgressBar = false;
        }

        for (int i = 0; i < NUM_KEY_MAPPINGS; i++)
        {
                if (strcmp(seq, "\033\n") == 0 &&
                    strcmp(keyMappings[i].seq, "^M") == 0) // ALT+ENTER
                {
                        event.type = keyMappings[i].eventType;
                        break;
                }

                if (strcmp(seq, keyMappings[i].seq) == 0 &&
                    strnlen(seq, MAX_SEQ_LEN) > 1) // ALT+something
                {
                        event.type = keyMappings[i].eventType;
                        break;
                }
        }

        // Handle numbers
        if (isdigit(event.key[0]))
        {
                if (digitsPressedCount < maxDigitsPressedCount)
                        digitsPressed[digitsPressedCount++] = event.key[0];
        }
        else
        {
                // Handle multiple digits, sometimes mixed with other keys
                for (int i = 0; i < MAX_SEQ_LEN; i++)
                {
                        if (isdigit(seq[i]))
                        {
                                if (digitsPressedCount < maxDigitsPressedCount)
                                        digitsPressed[digitsPressedCount++] =
                                            seq[i];
                        }
                        else
                        {
                                if (seq[i] == '\0')
                                        break;

                                if (seq[i] != settings.switchNumberedSong[0] &&
                                    seq[i] !=
                                        settings.hardSwitchNumberedSong[0] &&
                                    seq[i] != settings.hardEndOfPlaylist[0])
                                {
                                        memset(digitsPressed, '\0',
                                               sizeof(digitsPressed));
                                        digitsPressedCount = 0;
                                        break;
                                }
                                else if (seq[i] ==
                                         settings.hardEndOfPlaylist[0])
                                {
                                        event.type = EVENT_GOTOENDOFPLAYLIST;
                                        break;
                                }
                                else
                                {
                                        event.type = EVENT_GOTOSONG;
                                        break;
                                }
                        }
                }
        }

        // Handle song prev/next cooldown
        if (!cooldownElapsed &&
            (event.type == EVENT_NEXT || event.type == EVENT_PREV))
                event.type = EVENT_NONE;
        else if (event.type == EVENT_NEXT || event.type == EVENT_PREV)
                updateLastInputTime();

        // Handle seek/remove cooldown
        if (!cooldown2Elapsed &&
            (event.type == EVENT_REMOVE || event.type == EVENT_SEEKBACK ||
             event.type == EVENT_SEEKFORWARD))
                event.type = EVENT_NONE;
        else if (event.type == EVENT_REMOVE || event.type == EVENT_SEEKBACK ||
                 event.type == EVENT_SEEKFORWARD)
                updateLastInputTime();

        // Forget Numbers
        if (event.type != EVENT_GOTOSONG &&
            event.type != EVENT_GOTOENDOFPLAYLIST && event.type != EVENT_NONE)
        {
                memset(digitsPressed, '\0', sizeof(digitsPressed));
                digitsPressedCount = 0;
        }

        return event;
}

void setEndOfListReached(AppState *state)
{
        loadedNextSong = false;
        audioData.endOfListReached = true;
        usingSongDataA = false;
        currentSong = NULL;
        audioData.currentFileIndex = 0;
        audioData.restart = true;
        loadingdata.loadA = true;
        waitingForNext = true;

        pthread_mutex_lock(&dataSourceMutex);
        cleanupPlaybackDevice();
        pthread_mutex_unlock(&dataSourceMutex);
        stopped = true;
        refresh = true;

        if (isRepeatListEnabled())
                repeatList();
        else
        {
                emitPlaybackStoppedMpris();
                emitMetadataChanged("", "", "", "",
                                    "/org/mpris/MediaPlayer2/TrackList/NoTrack",
                                    NULL, 0);
                state->currentView = LIBRARY_VIEW;
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
            currentSong, length);
}

void notifySongSwitch(SongData *currentSongData, UISettings *ui)
{
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

                lastNotifiedId = currentSong->id;
        }
}

void determineSongAndNotify(UISettings *ui)
{
        SongData *currentSongData = NULL;

        bool isDeleted = determineCurrentSongData(&currentSongData);

        if (currentSongData && currentSong)
                currentSong->song.duration = currentSongData->duration;

        if (lastNotifiedId != currentSong->id)
        {
                if (!isDeleted)
                        notifySongSwitch(currentSongData, ui);
        }
}

// Checks conditions for refreshing player
bool shouldRefreshPlayer()
{
        return !skipping && !isEOFReached() && !isImplSwitchReached();
}

// Refreshes the player visually if conditions are met
void refreshPlayer(UIState *uis)
{
        int mutexResult = pthread_mutex_trylock(&switchMutex);

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
                printPlayer(getCurrentSongData(), elapsedSeconds, &settings,
                            &appState);
        }

        pthread_mutex_unlock(&switchMutex);
}

void resetListAfterDequeuingPlayingSong(AppState *state)
{
        startFromTop = true;

        if (lastPlayedId < 0)
                return;

        Node *node = findSelectedEntryById(&playlist, lastPlayedId);

        if (currentSong == NULL && node == NULL)
        {
                stopPlayback();

                loadedNextSong = false;
                audioData.endOfListReached = true;
                audioData.restart = true;

                emitMetadataChanged("", "", "", "",
                                    "/org/mpris/MediaPlayer2/TrackList/NoTrack",
                                    NULL, 0);
                emitPlaybackStoppedMpris();

                pthread_mutex_lock(&dataSourceMutex);
                cleanupPlaybackDevice();
                pthread_mutex_unlock(&dataSourceMutex);

                refresh = true;

                switchAudioImplementation();

                unloadSongA(state);
                unloadSongB(state);
                songWasRemoved = true;
                userData.currentSongData = NULL;

                audioData.currentFileIndex = 0;
                audioData.restart = true;
                waitingForNext = true;
                loadingdata.loadA = true;
                usingSongDataA = false;

                ma_data_source_uninit(&audioData);

                audioData.switchFiles = false;

                if (playlist.count == 0)
                        songToStartFrom = NULL;
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

        if (audioData.restart)
        {
                Node *lastSong = findSelectedEntryById(&playlist, lastPlayedId);
                startFromTop = false;

                if (lastSong == NULL)
                {
                        if (playlist.tail != NULL)
                                lastPlayedId = playlist.tail->id;
                        else
                        {
                                lastPlayedId = -1;
                                startFromTop = true;
                        }
                }
        }

        pthread_mutex_lock(&(playlist.mutex));

        firstEnqueuedEntry = enqueueSongs(entry, &(state->uiState));
        resetListAfterDequeuingPlayingSong(state);

        pthread_mutex_unlock(&(playlist.mutex));

        return firstEnqueuedEntry;
}

void playPreProcessing()
{
        wasEndOfList = false;
        if (audioData.endOfListReached)
                wasEndOfList = true;
}

void playPostProcessing()
{
        if ((songWasRemoved && currentSong != NULL))
        {
                songWasRemoved = false;
        }

        if (wasEndOfList)
        {
                skipOutOfOrder = false;
        }

        audioData.endOfListReached = false;
}

void handleGoToSong(AppState *state)
{
        bool canGoNext = (currentSong != NULL && currentSong->next != NULL);

        if (state->currentView == LIBRARY_VIEW)
        {
                FileSystemEntry *entry = getCurrentLibEntry();

                if (entry == NULL)
                        return;

                // Enqueue playlist
                if (pathEndsWith(entry->fullPath, "m3u") ||
                    pathEndsWith(entry->fullPath, "m3u8"))
                {
                        FileSystemEntry *firstEnqueuedEntry = NULL;
                        Node *prevTail = playlist.tail;

                        readM3UFile(entry->fullPath, &playlist, library);

                        if (prevTail != NULL && prevTail->next != NULL)
                        {
                                firstEnqueuedEntry = findCorrespondingEntry(
                                    library, prevTail->next->song.filePath);
                        }
                        else if (playlist.head != NULL)
                        {
                                firstEnqueuedEntry = findCorrespondingEntry(
                                    library, playlist.head->song.filePath);
                        }

                        autostartIfStopped(firstEnqueuedEntry);

                        markListAsEnqueued(library, &playlist);

                        deepCopyPlayListOntoList(&playlist, unshuffledPlaylist);
                }
                else
                        enqueue(state, entry); // Enqueue song
        }
        else if (state->currentView == SEARCH_VIEW)
        {
                pthread_mutex_lock(&(playlist.mutex));

                FileSystemEntry *entry = getCurrentSearchEntry();

                setChosenDir(entry);

                enqueueSongs(entry, &(state->uiState));

                resetListAfterDequeuingPlayingSong(state);

                pthread_mutex_unlock(&(playlist.mutex));
        }
        else if (state->currentView == PLAYLIST_VIEW)
        {
                if (digitsPressedCount == 0)
                {
                        if (isPaused() && currentSong != NULL &&
                            state->uiState.chosenNodeId == currentSong->id)
                        {
                                togglePause(&totalPauseSeconds, &pauseSeconds,
                                            &pause_time);
                        }
                        else
                        {
                                cleanupPlaybackDevice();

                                loadedNextSong = true;

                                nextSongNeedsRebuilding = false;

                                unloadSongA(state);
                                unloadSongB(state);

                                usingSongDataA = false;
                                audioData.currentFileIndex = 0;
                                loadingdata.loadA = true;

                                playPreProcessing();

                                playbackPlay(&totalPauseSeconds, &pauseSeconds);

                                Node *found = NULL;
                                findNodeInList(&playlist,
                                               state->uiState.chosenNodeId,
                                               &found);
                                play(found);

                                playPostProcessing();

                                skipOutOfOrder = false;
                                usingSongDataA = true;
                        }
                }
                else
                {
                        state->uiState.resetPlaylistDisplay = true;
                        int songNumber = getSongNumber(digitsPressed);
                        memset(digitsPressed, '\0', sizeof(digitsPressed));
                        digitsPressedCount = 0;

                        nextSongNeedsRebuilding = false;

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

void enqueueAndPlay(AppState *state)
{
        FileSystemEntry *firstEnqueuedEntry = NULL;

        bool wasEmpty = (playlist.count == 0);

        playPreProcessing();

        if (state->currentView == PLAYLIST_VIEW)
        {
                handleGoToSong(state);
                return;
        }

        if (state->currentView == LIBRARY_VIEW)
        {
                firstEnqueuedEntry = enqueue(state, getCurrentLibEntry());
        }

        if (state->currentView == SEARCH_VIEW)
        {
                FileSystemEntry *entry = getCurrentSearchEntry();
                firstEnqueuedEntry = enqueue(state, entry);

                setChosenDir(entry);
        }

        if (firstEnqueuedEntry && !wasEmpty)
        {
                Node *song =
                    findPathInPlaylist(firstEnqueuedEntry->fullPath, &playlist);

                loadedNextSong = true;

                nextSongNeedsRebuilding = false;

                cleanupPlaybackDevice();

                unloadSongA(state);
                unloadSongB(state);

                usingSongDataA = false;
                audioData.currentFileIndex = 0;
                loadingdata.loadA = true;

                playbackPlay(&totalPauseSeconds, &pauseSeconds);

                play(song);

                playPostProcessing();

                skipOutOfOrder = false;
                usingSongDataA = true;
        }
}

void gotoBeginningOfPlaylist(AppState *state)
{
        digitsPressed[0] = 1;
        digitsPressed[1] = '\0';
        digitsPressedCount = 1;
        handleGoToSong(state);
}

void gotoEndOfPlaylist(AppState *state)
{
        if (digitsPressedCount > 0)
        {
                handleGoToSong(state);
        }
        else
        {
                skipToLastSong();
        }
}

void handleInput(AppState *state)
{
        struct Event event = processInput(&(state->uiSettings));

        switch (event.type)
        {
        case EVENT_GOTOBEGINNINGOFPLAYLIST:
                gotoBeginningOfPlaylist(state);
                break;
        case EVENT_GOTOENDOFPLAYLIST:
                gotoEndOfPlaylist(state);
                break;
        case EVENT_GOTOSONG:
                handleGoToSong(state);
                break;
        case EVENT_PLAY_PAUSE:
                togglePause(&totalPauseSeconds, &pauseSeconds, &pause_time);
                break;
        case EVENT_TOGGLEVISUALIZER:
                toggleVisualizer(&settings, &(state->uiSettings));
                break;
        case EVENT_TOGGLEREPEAT:
                toggleRepeat(&(state->uiSettings));
                break;
        case EVENT_TOGGLEASCII:
                toggleAscii(&settings, &(state->uiSettings));
                break;
        case EVENT_SHUFFLE:
                toggleShuffle(&(state->uiSettings));
                emitShuffleChanged();
                break;
        case EVENT_CYCLECOLORMODE:
                cycleColorMode(&(state->uiSettings));
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
                adjustVolumePercent(5);
                emitVolumeChanged();
                break;
        case EVENT_VOLUME_DOWN:
                adjustVolumePercent(-5);
                emitVolumeChanged();
                break;
        case EVENT_NEXT:
                state->uiState.resetPlaylistDisplay = true;
                skipToNextSong(state);
                break;
        case EVENT_PREV:
                state->uiState.resetPlaylistDisplay = true;
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
                exportCurrentPlaylist(settings.path);
                break;
        case EVENT_UPDATELIBRARY:
                updateLibrary(settings.path);
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
                handleRemove();
                resetListAfterDequeuingPlayingSong(state);
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
                updatePlaylistToPlayingSong();
                state->uiState.resetPlaylistDisplay = true;
                break;
        case EVENT_MOVESONGUP:
                moveSongUp();
                break;
        case EVENT_MOVESONGDOWN:
                moveSongDown();
                break;
        case EVENT_ENQUEUEANDPLAY:
                enqueueAndPlay(state);
                break;
        case EVENT_STOP:
                stop();
                break;
        case EVENT_SORTLIBRARY:
                sortLibrary();
                break;

        default:
                fastForwarding = false;
                rewinding = false;
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
        refresh = true;
}

void updatePlayer(UIState *uis)
{
        struct winsize ws;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);

        // Check if window has changed size
        if (ws.ws_col != windowSize.ws_col || ws.ws_row != windowSize.ws_row)
        {
                uis->resizeFlag = 1;
                windowSize = ws;
        }

        // resizeFlag can also be set by handleResize
        if (uis->resizeFlag)
                resize(uis);
        else
        {
                refreshPlayer(uis);
        }
}

void loadAudioData(AppState *state)
{
        if (audioData.restart == true)
        {
                if (playlist.head != NULL &&
                    (waitingForPlaylist || waitingForNext))
                {
                        songLoading = true;

                        if (waitingForPlaylist)
                        {
                                currentSong = playlist.head;
                        }
                        else if (waitingForNext)
                        {
                                if (songToStartFrom != NULL)
                                {
                                        // Make sure it still exists in the
                                        // playlist
                                        findNodeInList(&playlist,
                                                       songToStartFrom->id,
                                                       &currentSong);

                                        songToStartFrom = NULL;
                                }
                                else if (lastPlayedId >= 0)
                                {
                                        currentSong = findSelectedEntryById(
                                            &playlist, lastPlayedId);
                                        if (currentSong != NULL &&
                                            currentSong->next != NULL)
                                                currentSong = currentSong->next;
                                }

                                if (currentSong == NULL)
                                {
                                        if (startFromTop)
                                        {
                                                currentSong = playlist.head;
                                                startFromTop = false;
                                        }
                                        else
                                                currentSong = playlist.tail;
                                }
                        }
                        audioData.restart = false;
                        waitingForPlaylist = false;
                        waitingForNext = false;
                        songWasRemoved = false;

                        if (isShuffleEnabled())
                                reshufflePlaylist();

                        unloadSongA(state);
                        unloadSongB(state);

                        int res = loadFirst(currentSong, state);

                        finishLoading();

                        if (res >= 0)
                        {
                                res = createAudioDevice();
                        }

                        if (res >= 0)
                        {
                                resetClock();
                                resumePlayback();
                        }
                        else
                        {
                                setEndOfListReached(state);
                        }

                        loadedNextSong = false;
                        nextSong = NULL;
                        refresh = true;

                        clock_gettime(CLOCK_MONOTONIC, &start_time);
                }
        }
        else if (currentSong != NULL &&
                 (nextSongNeedsRebuilding || nextSong == NULL) && !songLoading)
        {
                loadNextSong();
                determineSongAndNotify(&(state->uiSettings));
        }
}

void tryLoadNext()
{
        songHasErrors = false;
        clearingErrors = true;

        if (tryNextSong == NULL && currentSong != NULL)
                tryNextSong = currentSong->next;
        else if (tryNextSong != NULL)
                tryNextSong = tryNextSong->next;

        if (tryNextSong != NULL)
        {
                songLoading = true;
                loadingdata.loadA = !usingSongDataA;
                loadingdata.loadingFirstDecoder = false;
                loadSong(tryNextSong, &loadingdata);
        }
        else
        {
                clearingErrors = false;
        }
}

void handleSkipOutOfOrder(void)
{
        if (!skipOutOfOrder && !isRepeatEnabled())
        {
                setCurrentSongToNext();
        }
        else
        {
                skipOutOfOrder = false;
        }
}

void prepareNextSong(AppState *state)
{
        resetClock();
        handleSkipOutOfOrder();
        finishLoading();

        nextSong = NULL;
        refresh = true;

        if (!isRepeatEnabled() || currentSong == NULL)
        {
                unloadPreviousSong(state);
        }

        if (currentSong == NULL)
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
                determineSongAndNotify(&(state->uiSettings));
        }
}

void handleSkipFromStopped()
{
        // If we don't do this the song gets loaded in the wrong slot
        if (skipFromStopped)
        {
                usingSongDataA = !usingSongDataA;
                skipOutOfOrder = false;
                skipFromStopped = false;
        }
}

void updatePlayerStatus(AppState *state)
{
        updatePlayer(&(state->uiState));

        if (playlist.head != NULL)
        {
                if ((skipFromStopped || !loadedNextSong ||
                     nextSongNeedsRebuilding) &&
                    !audioData.endOfListReached)
                {
                        loadAudioData(state);
                }

                if (songHasErrors)
                        tryLoadNext();

                if (isPlaybackDone())
                {
                        resetStartTime();
                        prepareNextSong(state);
                        switchAudioImplementation();
                }
        }
        else
        {
                setEOFNotReached();
        }
}

void processDBusEvents(void)
{
        while (g_main_context_pending(global_main_context))
        {
                g_main_context_iteration(global_main_context, FALSE);
        }
}

gboolean mainloop_callback(gpointer data)
{
        (void)data;

        calcElapsedTime();

        handleInput(&appState);

        updateCounter++;

        // Different views run at different speeds to lower the impact on system
        // requirements
        if ((updateCounter % 2 == 0 && appState.currentView == SEARCH_VIEW) ||
            (appState.currentView == TRACK_VIEW || appState.uiState.miniMode) ||
            updateCounter % 3 == 0)
        {
                processDBusEvents();

                updatePlayerStatus(&appState);
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
        resetStartTime();

        userData.currentSongData = NULL;
        userData.songdataA = NULL;
        userData.songdataB = NULL;
        userData.songdataADeleted = true;
        userData.songdataBDeleted = true;

        int res = 0;

        if (song != NULL)
        {
                audioData.currentFileIndex = 0;
                loadingdata.loadA = true;
                res = loadFirst(song, state);

                if (res >= 0)
                {
                        res = createAudioDevice();
                }
                if (res >= 0)
                {
                        resumePlayback();
                }

                if (res < 0)
                        setEndOfListReached(state);
        }

        if (song == NULL || res < 0)
        {
                song = NULL;
                if (!waitingForNext)
                        waitingForPlaylist = true;
        }

        loadedNextSong = false;
        nextSong = NULL;
        refresh = true;

        clock_gettime(CLOCK_MONOTONIC, &start_time);

        main_loop = g_main_loop_new(NULL, FALSE);

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
        pthread_mutex_lock(&dataSourceMutex);

        resetAllDecoders();

        if (isContextInitialized)
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

        if (!userData.songdataADeleted)
        {
                userData.songdataADeleted = true;
                unloadSongData(&(loadingdata.songdataA), &appState);
        }
        if (!userData.songdataBDeleted)
        {
                userData.songdataBDeleted = true;
                unloadSongData(&(loadingdata.songdataB), &appState);
        }

#ifdef CHAFA_VERSION_1_16
        retire_passthrough_workarounds_tmux();
#endif

        freeSearchResults();
        cleanupMpris();
        restoreTerminalMode();
        enableInputBuffering();
        setConfig(&settings, &(appState.uiSettings));
        saveFavoritesPlaylist(settings.path);
        saveLastUsedPlaylist();
        deleteCache(appState.tmpCache);
        freeMainDirectoryTree(&appState);
        deletePlaylist(&playlist);
        deletePlaylist(unshuffledPlaylist);
        deletePlaylist(favoritesPlaylist);
        free(favoritesPlaylist);
        free(unshuffledPlaylist);
        setDefaultTextColor();
        pthread_mutex_destroy(&(loadingdata.mutex));
        pthread_mutex_destroy(&(playlist.mutex));
        pthread_mutex_destroy(&(switchMutex));
        pthread_mutex_unlock(&dataSourceMutex);
        pthread_mutex_destroy(&(dataSourceMutex));
        freeVisuals();
#ifdef USE_DBUS
        cleanupDbusConnection();
#endif

#ifdef DEBUG
        fclose(logFile);
#endif
        if (freopen("/dev/stderr", "w", stderr) == NULL)
        {
                perror("freopen error");
        }

        printf("\n");
        showCursor();
        exitAlternateScreenBuffer();
        if (appState.uiSettings.mouseEnabled)
                disableTerminalMouseButtons();

        if (appState.uiSettings.trackTitleAsWindowTitle)
                restoreTerminalWindowTitle();

        if (noMusicFound)
        {
                printf("No Music found.\n");
                printf("Please make sure the path is set correctly. \n");
                printf("To set it type: kew path \"/path/to/Music\". \n");
        }
        else if (noPlaylist)
        {
                printf("Music not found.\n");
        }

        fflush(stdout);
}

void run(AppState *state, bool startPlaying)
{
        if (unshuffledPlaylist == NULL)
        {

                unshuffledPlaylist = malloc(sizeof(PlayList));
                *unshuffledPlaylist = deepCopyPlayList(&playlist);
        }

        if (state->uiSettings.saveRepeatShuffleSettings)
        {
                if (state->uiSettings.repeatState == 1)
                        toggleRepeat(&(state->uiSettings));
                if (state->uiSettings.repeatState == 2)
                {
                        toggleRepeat(&(state->uiSettings));
                        toggleRepeat(&(state->uiSettings));
                }
                if (state->uiSettings.shuffleEnabled)
                        toggleShuffle(&(state->uiSettings));
        }

        if (playlist.head == NULL)
        {
                state->currentView = LIBRARY_VIEW;
        }

        initMpris();

        if (startPlaying)
                currentSong = playlist.head;
        else if (playlist.count > 0)
                waitingForNext = true;

        initFirstPlay(currentSong, state);
        clearScreen();
        fflush(stdout);
}

void handleResize(int sig)
{
        (void)sig;
        appState.uiState.resizeFlag = 1;
}

void resetResizeFlag(int sig)
{
        (void)sig;
        appState.uiState.resizeFlag = 0;
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
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &windowSize);
        enableScrolling();
        setNonblockingMode();
        state->tmpCache = createCache();
        c_strcpy(loadingdata.filePath, "", sizeof(loadingdata.filePath));
        loadingdata.songdataA = NULL;
        loadingdata.songdataB = NULL;
        loadingdata.loadA = true;
        loadingdata.loadingFirstDecoder = true;
        audioData.restart = true;
        userData.songdataADeleted = true;
        userData.songdataBDeleted = true;
        unsigned int seed = (unsigned int)time(NULL);
        srand(seed);
        pthread_mutex_init(&dataSourceMutex, NULL);
        pthread_mutex_init(&switchMutex, NULL);
        pthread_mutex_init(&(loadingdata.mutex), NULL);
        pthread_mutex_init(&(playlist.mutex), NULL);
        createLibrary(&settings, state);
        setlocale(LC_ALL, "");
        setlocale(LC_CTYPE, "");
        fflush(stdout);

#ifdef DEBUG
        // g_setenv("G_MESSAGES_DEBUG", "all", TRUE);
        logFile = freopen("error.log", "w", stderr);
        if (logFile == NULL)
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

        loadLastUsedPlaylist();
        markListAsEnqueued(library, &playlist);

        resetListAfterDequeuingPlayingSong(state);

        audioData.restart = true;
        audioData.endOfListReached = true;
        loadedNextSong = false;

        state->currentView = LIBRARY_VIEW;

        run(state, false);
}

void playFavoritesPlaylist(AppState *state)
{
        if (favoritesPlaylist->count == 0)
        {
                printf("Couldn't find any songs in the special playlist. Add a "
                       "song by pressing '.' while it's playing. \n");
                exit(0);
        }

        init(state);
        deepCopyPlayListOntoList(favoritesPlaylist, &playlist);
        shufflePlaylist(&playlist);
        markListAsEnqueued(library, &playlist);
        run(state, true);
}

void playAll(AppState *state)
{
        init(state);
        FileSystemEntry *library = getLibrary();
        createPlayListFromFileSystemEntry(library, &playlist, MAX_FILES);
        if (playlist.count == 0)
        {
                exit(0);
        }
        shufflePlaylist(&playlist);
        markListAsEnqueued(library, &playlist);
        run(state, true);
}

void playAllAlbums(AppState *state)
{
        init(state);
        FileSystemEntry *library = getLibrary();
        addShuffledAlbumsToPlayList(library, &playlist, MAX_FILES);
        if (playlist.count == 0)
        {
                exit(0);
        }
        markListAsEnqueued(library, &playlist);
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

void handleOptions(int *argc, char *argv[], UISettings *ui)
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
                        exactSearch = true;
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
            "Music",  "Música", "Musique", "Musik",  "Musica", "Muziek",
            "Музыка", "音乐",   "音楽",    "음악",   "موسيقى", "संगीत",
            "Müzik",  "Musikk", "Μουσική", "Muzyka", "Hudba",  "Musiikki",
            "Zene",   "Muzică", "เพลง",    "מוזיקה"};

        char path[PATH_MAX];
        int found = 0;
        int result = -1;
        char choice[2];

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
                                c_strcpy(settings.path, path,
                                         sizeof(settings.path));
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
                        c_strcpy(settings.path, path, sizeof(settings.path));
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
        state->tmpCache = NULL;
}

void initSettings(AppState *appState, AppSettings *settings)
{
        getConfig(settings, &(appState->uiSettings));
        userData.replayGainCheckFirst =
            appState->uiSettings.replayGainCheckFirst;
        mapSettingsToKeys(settings, &(appState->uiSettings), keyMappings);
        enableMouse(&(appState->uiSettings));
        setTrackTitleAsWindowTitle(&(appState->uiSettings));
}

// Copies default themes to config dir if they aren't alread there
bool ensureDefaultThemes(void)
{
        bool copied = false;

        char *configPath = getConfigPath();
        if (!configPath)
                return false;

        char themesPath[MAXPATHLEN];
        if (snprintf(themesPath, sizeof(themesPath), "%s/themes", configPath) >=
            (int)sizeof(themesPath))
        {
                free(configPath);
                return false;
        }

        // Check if user themes directory exists
        struct stat st;
        if (stat(themesPath, &st) == -1)
        {
                mkdir(themesPath, 0755);

                char *systemThemes = PREFIX "/share/kew/themes";

                // Copy themes from systemThemes to themesPath
                DIR *dir = opendir(systemThemes);
                if (dir)
                {
                        struct dirent *entry;
                        while ((entry = readdir(dir)) != NULL)
                        {
                                if (entry->d_type == DT_REG &&
                                    (strstr(entry->d_name, ".theme") ||
                                     strstr(entry->d_name, ".txt")))
                                {
                                        char src[MAXPATHLEN], dst[MAXPATHLEN];

                                        // Check if paths would be truncated
                                        if (snprintf(src, sizeof(src), "%s/%s",
                                                     systemThemes,
                                                     entry->d_name) >=
                                            (int)sizeof(src))
                                        {
                                                continue; // Skip this file if
                                                          // path too long
                                        }
                                        if (snprintf(dst, sizeof(dst), "%s/%s",
                                                     themesPath,
                                                     entry->d_name) >=
                                            (int)sizeof(dst))
                                        {
                                                continue; // Skip this file if
                                                          // path too long
                                        }

                                        copyFile(src, dst);
                                        copied = true;
                                }
                        }
                        closedir(dir);
                }
        }

        free(configPath);

        return copied;
}

void initTheme(int argc, char *argv[], UISettings *ui)
{
        bool themeLoaded = false;

        // Command-line theme handling
        if (argc == 3 && strcmp(argv[1], "theme") == 0)
        {
                // Try to load the user-specified theme
                if (loadTheme(&appState, &settings, argv[2], false) > 0)
                {
                        ui->colorMode = COLOR_MODE_THEME;
                        initDefaultState(&appState);
                        themeLoaded = true;
                }
                else
                {
                        // Failed to load user theme → fall back to
                        // terminal/ANSI
                        if (ui->colorMode == COLOR_MODE_THEME)
                        {
                                ui->colorMode = COLOR_MODE_TERMINAL;
                        }
                }
        }
        else if (ui->colorMode == COLOR_MODE_THEME)
        {
                // If UI has a themeName stored, try to load it
                if (loadTheme(&appState, &settings, ui->themeName, false) > 0)
                {
                        ui->colorMode = COLOR_MODE_THEME;
                        themeLoaded = true;
                }
        }

        // If still in terminal mode, load default ANSI theme
        if (ui->colorMode == COLOR_MODE_TERMINAL)
        {
                // Load "default" ANSI theme, but don't overwrite
                // settings->theme
                if (loadTheme(&appState, &settings, "default", true))
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
        initState(&appState);

        UISettings *ui = &(appState.uiSettings);

        exitIfAlreadyRunning();

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

        initSettings(&appState, &settings);

        if (argc == 3 && (strcmp(argv[1], "path") == 0))
        {
                c_strcpy(settings.path, argv[2], sizeof(settings.path));
                setConfig(&settings, ui);
                exit(0);
        }

        enterAlternateScreenBuffer();
        atexit(cleanupOnExit);

        if (settings.path[0] == '\0')
        {
                setMusicPath();
        }

        handleOptions(&argc, argv, ui);
        loadFavoritesPlaylist(settings.path);
        ensureDefaultThemes();
        initTheme(argc, argv, ui);

        if (argc == 1)
        {
                initDefaultState(&appState);
        }
        else if (argc == 2 && strcmp(argv[1], "all") == 0)
        {
                playAll(&appState);
        }
        else if (argc == 2 && strcmp(argv[1], "albums") == 0)
        {
                playAllAlbums(&appState);
        }
        else if (argc == 2 && strcmp(argv[1], ".") == 0 &&
                 favoritesPlaylist->count != 0)
        {
                playFavoritesPlaylist(&appState);
        }
        else if (argc >= 2)
        {
                init(&appState);
                makePlaylist(argc, argv, exactSearch, settings.path);
                if (playlist.count == 0)
                {
                        noPlaylist = true;
                        exit(0);
                }
                markListAsEnqueued(library, &playlist);
                run(&appState, true);
        }

        return 0;
}
