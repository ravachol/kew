/* kew - a command-line music player
Copyright (C) 2022 Ravachol

http://github.com/ravachol/kew

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

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef __USE_POSIX
#define __USE_POSIX
#endif

#include <dirent.h>
#include <fcntl.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib-unix.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <time.h>
#include <unistd.h>
#include "cache.h"
#include "events.h"
#include "file.h"
#include "mpris.h"
#include "player.h"
#include "playerops.h"
#include "playlist.h"
#include "settings.h"
#include "sound.h"
#include "soundcommon.h"
#include "songloader.h"
#include "utils.h"

// #define DEBUG 1
#define MAX_SEQ_LEN 1024    // Maximum length of sequence buffer
#define MAX_TMP_SEQ_LEN 256 // Maximum length of temporary sequence buffer
#define COOLDOWN_MS 500
#define COOLDOWN2_MS 100

FILE *logFile = NULL;
struct winsize windowSize;
static bool eventProcessed = false;
char digitsPressed[MAX_SEQ_LEN];
int digitsPressedCount = 0;
int maxDigitsPressedCount = 9;
bool gPressed = false;
bool loadingAudioData = false;
bool goingToSong = false;
bool startFromTop = false;
bool exactSearch = false;
AppSettings settings;

bool isCooldownElapsed(int milliSeconds)
{
        struct timespec currentTime;
        clock_gettime(CLOCK_MONOTONIC, &currentTime);
        double elapsedMilliseconds = (currentTime.tv_sec - lastInputTime.tv_sec) * 1000.0 +
                                     (currentTime.tv_nsec - lastInputTime.tv_nsec) / 1000000.0;

        return elapsedMilliseconds >= milliSeconds;
}

struct Event processInput()
{
        struct Event event;
        event.type = EVENT_NONE;
        event.key = '\0';
        bool cooldownElapsed = false;
        bool cooldown2Elapsed = false;

        if (!isInputAvailable())
        {
                flushSeek();
                return event;
        }

        if (isCooldownElapsed(COOLDOWN_MS) && !eventProcessed)
                cooldownElapsed = true;

        if (isCooldownElapsed(COOLDOWN2_MS) && !eventProcessed)
                cooldown2Elapsed = true;

        int seqLength = 0;
        char seq[MAX_SEQ_LEN];
        seq[0] = '\0'; // Set initial value
        int keyReleased = 0;

        // Find input
        while (isInputAvailable())
        {
                char tmpSeq[MAX_TMP_SEQ_LEN];

                seqLength = seqLength + readInputSequence(tmpSeq, sizeof(tmpSeq));

                // Release most keys directly, seekbackward and seekforward can be read continuously
                if (seqLength <= 0 && strcmp(seq, settings.seekBackward) != 0 && strcmp(seq, settings.seekForward) != 0)
                {
                        keyReleased = 1;
                        break;
                }

                if (strlen(seq) + strlen(tmpSeq) >= MAX_SEQ_LEN)
                {
                        break;
                }

                strcat(seq, tmpSeq);

                c_sleep(10);

                // This slows the continous reads down to not get a a too fast scrolling speed
                if (strcmp(seq, settings.hardScrollUp) == 0 || strcmp(seq, settings.hardScrollDown) == 0 || strcmp(seq, settings.scrollUpAlt) == 0 ||
                    strcmp(seq, settings.scrollDownAlt) == 0 || strcmp(seq, settings.seekBackward) == 0 || strcmp(seq, settings.seekForward) == 0 ||
                    strcmp(seq, settings.hardNextPage) == 0 || strcmp(seq, settings.hardPrevPage) == 0)
                {
                        // Do dummy reads to prevent scrolling continuing after we release the key
                        readInputSequence(tmpSeq, 3);
                        readInputSequence(tmpSeq, 3);
                        keyReleased = 0;
                        break;
                }

                keyReleased = 0;
        }

        if (keyReleased)
                return event;

        eventProcessed = true;
        event.type = EVENT_NONE;
        event.key = seq[0];

        // Map keys to events
        EventMapping keyMappings[] = {{settings.scrollUpAlt, EVENT_SCROLLPREV},
                                      {settings.scrollDownAlt, EVENT_SCROLLNEXT},
                                      {settings.nextTrackAlt, EVENT_NEXT},
                                      {settings.previousTrackAlt, EVENT_PREV},
                                      {settings.volumeUp, EVENT_VOLUME_UP},
                                      {settings.volumeUpAlt, EVENT_VOLUME_UP},
                                      {settings.volumeDown, EVENT_VOLUME_DOWN},
                                      {settings.togglePause, EVENT_PLAY_PAUSE},
                                      {settings.quit, EVENT_QUIT},
                                      {settings.toggleShuffle, EVENT_SHUFFLE},
                                      {settings.toggleCovers, EVENT_TOGGLECOVERS},
                                      {settings.toggleVisualizer, EVENT_TOGGLEVISUALIZER},
                                      {settings.toggleAscii, EVENT_TOGGLEBLOCKS},
                                      {settings.switchNumberedSong, EVENT_GOTOSONG},
                                      {settings.seekBackward, EVENT_SEEKBACK},
                                      {settings.seekForward, EVENT_SEEKFORWARD},
                                      {settings.toggleRepeat, EVENT_TOGGLEREPEAT},
                                      {settings.savePlaylist, EVENT_EXPORTPLAYLIST},
                                      {settings.toggleColorsDerivedFrom, EVENT_TOGGLE_PROFILE_COLORS},
                                      {settings.addToMainPlaylist, EVENT_ADDTOMAINPLAYLIST},
                                      {settings.updateLibrary, EVENT_UPDATELIBRARY},
                                      {settings.hardPlayPause, EVENT_PLAY_PAUSE},
                                      {settings.hardPrev, EVENT_PREV},
                                      {settings.hardNext, EVENT_NEXT},
                                      {settings.hardSwitchNumberedSong, EVENT_GOTOSONG},
                                      {settings.hardScrollUp, EVENT_SCROLLPREV},
                                      {settings.hardScrollDown, EVENT_SCROLLNEXT},
                                      {settings.hardShowInfo, EVENT_SHOWINFO},
                                      {settings.hardShowInfoAlt, EVENT_SHOWINFO},
                                      {settings.hardShowKeys, EVENT_SHOWKEYBINDINGS},
                                      {settings.hardShowKeysAlt, EVENT_SHOWKEYBINDINGS},
                                      {settings.hardEndOfPlaylist, EVENT_GOTOENDOFPLAYLIST},
                                      {settings.hardShowTrack, EVENT_SHOWTRACK},
                                      {settings.hardShowTrackAlt, EVENT_SHOWTRACK},
                                      {settings.hardShowLibrary, EVENT_SHOWLIBRARY},
                                      {settings.hardShowLibraryAlt, EVENT_SHOWLIBRARY},
                                      {settings.hardNextPage, EVENT_NEXTPAGE},
                                      {settings.hardPrevPage, EVENT_PREVPAGE},
                                      {settings.hardRemove, EVENT_REMOVE}};

        int numKeyMappings = sizeof(keyMappings) / sizeof(EventMapping);

        // Set event for pressed key
        for (int i = 0; i < numKeyMappings; i++)
        {
                if (strcmp(seq, keyMappings[i].seq) == 0)
                {
                        event.type = keyMappings[i].eventType;
                        break;
                }
        }

        // Handle gg
        if (event.key == 'g' && event.type == EVENT_NONE)
        {
                if (gPressed)
                {
                        event.type = EVENT_GOTOBEGINNINGOFPLAYLIST;
                        gPressed = false;
                }
                else
                {
                        gPressed = true;
                }
        }

        // Handle numbers
        if (isdigit(event.key))
        {
                if (digitsPressedCount < maxDigitsPressedCount)
                        digitsPressed[digitsPressedCount++] = event.key;
        }
        else
        {
                // Handle multiple digits, sometimes mixed with other keys
                for (int i = 0; i < MAX_SEQ_LEN; i++)
                {
                        if (isdigit(seq[i]))
                        {
                                if (digitsPressedCount < maxDigitsPressedCount)
                                        digitsPressed[digitsPressedCount++] = seq[i];
                        }
                        else
                        {
                                if (seq[i] == '\0')
                                        break;

                                if (seq[i] != settings.switchNumberedSong[0] &&
                                    seq[i] != settings.hardSwitchNumberedSong[0] &&
                                    seq[i] != settings.hardEndOfPlaylist[0])
                                {
                                        memset(digitsPressed, '\0', sizeof(digitsPressed));
                                        digitsPressedCount = 0;
                                        break;
                                }
                                else if (seq[i] == settings.hardEndOfPlaylist[0])
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
        if (!cooldownElapsed && (event.type == EVENT_NEXT || event.type == EVENT_PREV))
                event.type = EVENT_NONE;
        else if (event.type == EVENT_NEXT || event.type == EVENT_PREV)
                updateLastInputTime();

        // Handle seek/remove cooldown
        if (!cooldown2Elapsed && (event.type == EVENT_REMOVE || event.type == EVENT_SEEKBACK || event.type == EVENT_SEEKFORWARD))
                event.type = EVENT_NONE;
        else if (event.type == EVENT_REMOVE || event.type == EVENT_SEEKBACK || event.type == EVENT_SEEKFORWARD)
                updateLastInputTime();

        // Forget Numbers
        if (event.type != EVENT_GOTOSONG && event.type != EVENT_GOTOENDOFPLAYLIST && event.type != EVENT_NONE)
        {
                memset(digitsPressed, '\0', sizeof(digitsPressed));
                digitsPressedCount = 0;
        }

        // Forget g pressed
        if (event.key != 'g')
        {
                gPressed = false;
        }

        return event;
}

void setEndOfListReached()
{
        appState.currentView = LIBRARY_VIEW;
        loadedNextSong = false;
        audioData.endOfListReached = true;
        usingSongDataA = false;
        audioData.currentFileIndex = 0;
        audioData.restart = true;
        loadingdata.loadA = true;
        emitMetadataChanged("", "", "", "", "/org/mpris/MediaPlayer2/TrackList/NoTrack", NULL, 0);
        emitPlaybackStoppedMpris();
        pthread_mutex_lock(&dataSourceMutex);
        cleanupPlaybackDevice();
        pthread_mutex_unlock(&dataSourceMutex);
        refresh = true;
        chosenRow = playlist.count - 1;
}

void finishLoading()
{
        int maxNumTries = 20;
        int numtries = 0;

        while (!loadedNextSong && !loadingFailed && numtries < maxNumTries)
        {
                c_sleep(100);
                numtries++;
        }

        loadedNextSong = true;
}

void resetTimeCount()
{
        elapsedSeconds = 0.0;
        pauseSeconds = 0.0;
        totalPauseSeconds = 0.0;
}

void notifySongSwitch()
{
        SongData *currentSongData = (audioData.currentFileIndex == 0) ? userData.songdataA : userData.songdataB;
        bool isDeleted = (audioData.currentFileIndex == 0) ? userData.songdataADeleted == true : userData.songdataBDeleted == true;

        if (isDeleted == false && currentSongData != NULL && currentSongData->hasErrors == 0 && currentSongData->metadata && strlen(currentSongData->metadata->title) > 0)
                displaySongNotification(currentSongData->metadata->artist, currentSongData->metadata->title, currentSongData->coverArtPath);
}

void prepareNextSong()
{
        if (!skipOutOfOrder && !isRepeatEnabled())
        {
                if (currentSong != NULL)
                        lastPlayedId = currentSong->id;
                currentSong = getNextSong();
        }
        else
        {
                skipOutOfOrder = false;
        }

        finishLoading();
        resetTimeCount();

        nextSong = NULL;
        refresh = true;

        if (!isRepeatEnabled() || currentSong == NULL)
        {
                unloadPreviousSong();
        }

        if (currentSong == NULL)
        {
                if (quitAfterStopping)
                        quit();
                else
                        setEndOfListReached();
        }
        else
        {
                notifySongSwitch();
        }

        clock_gettime(CLOCK_MONOTONIC, &start_time);
}

void refreshPlayer()
{
        if (pthread_mutex_trylock(&switchMutex) != 0)
        {
                return;
        }

        SongData *songData = (audioData.currentFileIndex == 0) ? userData.songdataA : userData.songdataB;
        bool isDeleted = (audioData.currentFileIndex == 0) ? userData.songdataADeleted == true : userData.songdataBDeleted == true;

        if (!skipping && !isEOFReached() && !isImplSwitchReached())
        {
                if (refresh && songData != NULL && isDeleted == false &&
                    songData->hasErrors == false && currentSong != NULL && songData->metadata != NULL)
                {
                        gint64 length = llround(songData->duration * G_USEC_PER_SEC);
                        // update mpris
                        emitMetadataChanged(
                            songData->metadata->title,
                            songData->metadata->artist,
                            songData->metadata->album,
                            songData->coverArtPath,
                            songData->trackId != NULL ? songData->trackId : "", currentSong,
                            length);
                }

                if (isDeleted || songData->hasErrors)
                        songData = NULL;

                printPlayer(songData, elapsedSeconds, &settings);
        }

        pthread_mutex_unlock(&switchMutex);
}

void handleGoToSong()
{
        if (goingToSong)
                return;

        goingToSong = true;

        if (appState.currentView != LIBRARY_VIEW)
        {
                if (digitsPressedCount == 0)
                {
                        loadedNextSong = true;

                        playlistNeedsUpdate = false;
                        nextSongNeedsRebuilding = false;

                        skipToSong(chosenNodeId);
                        audioData.endOfListReached = false;
                }
                else
                {
                        resetPlaylistDisplay = true;
                        int songNumber = atoi(digitsPressed);
                        memset(digitsPressed, '\0', sizeof(digitsPressed));
                        digitsPressedCount = 0;

                        playlistNeedsUpdate = false;
                        nextSongNeedsRebuilding = false;

                        skipToNumberedSong(songNumber);
                }
        }
        else
        {
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

                enqueueSongs();

                pthread_mutex_unlock(&(playlist.mutex));
        }

        goingToSong = false;
}

void gotoBeginningOfPlaylist()
{
        digitsPressed[0] = 1;
        digitsPressed[1] = '\0';
        digitsPressedCount = 1;
        handleGoToSong();
}

void gotoEndOfPlaylist()
{
        if (digitsPressedCount > 0)
        {
                handleGoToSong();
        }
        else
        {
                skipToLastSong();
        }
}

void handleInput()
{
        struct Event event = processInput();

        switch (event.type)
        {
        case EVENT_GOTOBEGINNINGOFPLAYLIST:
                gotoBeginningOfPlaylist();
                break;
        case EVENT_GOTOENDOFPLAYLIST:
                gotoEndOfPlaylist();
                break;
        case EVENT_GOTOSONG:
                handleGoToSong();
                break;
        case EVENT_PLAY_PAUSE:
                togglePause(&totalPauseSeconds, &pauseSeconds, &pause_time);
                break;
        case EVENT_TOGGLEVISUALIZER:
                toggleVisualizer(&settings);
                break;
        case EVENT_TOGGLEREPEAT:
                toggleRepeat();
                break;
        case EVENT_TOGGLECOVERS:
                toggleCovers(&settings);
                break;
        case EVENT_TOGGLEBLOCKS:
                toggleBlocks(&settings);
                break;
        case EVENT_SHUFFLE:
                toggleShuffle();
                break;
        case EVENT_TOGGLE_PROFILE_COLORS:
                toggleColors(&settings);
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
                break;
        case EVENT_VOLUME_DOWN:
                adjustVolumePercent(-5);
                break;
        case EVENT_NEXT:
                resetPlaylistDisplay = true;
                skipToNextSong();
                break;
        case EVENT_PREV:
                resetPlaylistDisplay = true;
                skipToPrevSong();
                break;
        case EVENT_SEEKBACK:
                seekBack();
                break;
        case EVENT_SEEKFORWARD:
                seekForward();
                break;
        case EVENT_ADDTOMAINPLAYLIST:
                addToSpecialPlaylist();
                break;
        case EVENT_EXPORTPLAYLIST:
                savePlaylist(settings.path);
                break;
        case EVENT_UPDATELIBRARY:
                updateLibrary(settings.path);
                break;
        case EVENT_SHOWKEYBINDINGS:
                toggleShowKeyBindings();
                break;
        case EVENT_SHOWINFO:
                toggleShowPlaylist();
                break;
        case EVENT_SHOWLIBRARY:
                toggleShowLibrary();
                break;
        case EVENT_NEXTPAGE:
                flipNextPage();
                break;
        case EVENT_PREVPAGE:
                flipPrevPage();
                break;
        case EVENT_REMOVE:
                handleRemove();
                break;
        case EVENT_SHOWTRACK:
                showTrack();
                break;
        default:
                fastForwarding = false;
                rewinding = false;
                break;
        }
        eventProcessed = false;
}

void resize()
{
        alarm(1); // Timer
        while (resizeFlag)
        {
                resizeFlag = 0;
                c_sleep(100);
        }
        alarm(0); // Cancel timer
        printf("\033[1;1H");
        clearScreen();
        refresh = true;
}

void updatePlayer()
{
        struct winsize ws;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);

        // check if window has changed size
        if (ws.ws_col != windowSize.ws_col || ws.ws_row != windowSize.ws_row)
        {
                resizeFlag = 1;
                windowSize = ws;
        }

        // resizeFlag can also be set by handleResize
        if (resizeFlag)
                resize();
        else
        {
                refreshPlayer();
        }
}

void loadAudioData()
{
        loadingAudioData = true;

        if (audioData.restart == true)
        {
                if (playlist.head != NULL && (waitingForPlaylist || waitingForNext))
                {
                        songLoading = true;

                        if (waitingForPlaylist)
                        {
                                currentSong = playlist.head;
                        }
                        else if (waitingForNext)
                        {
                                if (lastPlayedId >= 0)
                                {
                                        currentSong = findSelectedEntryById(&playlist, lastPlayedId);
                                        if (currentSong != NULL && currentSong->next != NULL)
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

                        int res = loadFirst(currentSong);

                        if (res >= 0)
                        {
                                res = createAudioDevice(&userData);
                        }

                        if (res >= 0)
                        {
                                resumePlayback();                            
                        }
                        else
                        {
                                setEndOfListReached();
                        }

                        playlistNeedsUpdate = false;
                        loadedNextSong = false;
                        nextSong = NULL;
                        refresh = true;

                        clock_gettime(CLOCK_MONOTONIC, &start_time);                        
                }
        }
        else if (currentSong != NULL && (nextSongNeedsRebuilding || nextSong == NULL) && !songLoading)
        {
                songLoading = true;
                nextSongNeedsRebuilding = false;
                tryNextSong = currentSong->next;
                loadingdata.loadA = !usingSongDataA;
                nextSong = getListNext(currentSong);
                loadingdata.loadingFirstDecoder = false;
                loadSong(nextSong, &loadingdata);
        }

        loadingAudioData = false;
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

gboolean mainloop_callback(gpointer data)
{
        (void)data;
        
        calcElapsedTime();
        handleInput();

        // Process GDBus events in the global_main_context
        while (g_main_context_pending(global_main_context))
        {
                g_main_context_iteration(global_main_context, FALSE);
        }

        updatePlayer();

        if (playlist.head != NULL)
        {
                if (loadingAudioData == false && (!loadedNextSong || nextSongNeedsRebuilding) && !audioData.endOfListReached)
                {
                        loadAudioData();
                }

                if (songHasErrors)
                        tryLoadNext();

                if (isPlaybackDone())
                {
                        updateLastSongSwitchTime();
                        prepareNextSong();
                        if (!doQuit)
                                switchAudioImplementation();
                }
        }

        if (doQuit)
        {
                g_main_loop_quit(main_loop);
                return FALSE;
        }

        return TRUE;
}

static gboolean on_sigint(gpointer user_data)
{
        doQuit = true;
        GMainLoop *loop = (GMainLoop *)user_data;
        g_main_loop_quit(loop);
        return G_SOURCE_REMOVE; // Remove the signal source
}

void play(Node *song)
{
        updateLastInputTime();
        updateLastSongSwitchTime();

        int res = 0;

        if (song != NULL)
        {
                audioData.currentFileIndex = 0;
                loadingdata.loadA = true;
                res = loadFirst(song);

                if (res >= 0)
                {
                        res = createAudioDevice(&userData);
                }

                if (res < 0)
                        setEndOfListReached();
        }
                
        if (song == NULL || res < 0)
        {
                song = NULL;
                waitingForPlaylist = true;
        }

        loadedNextSong = false;
        nextSong = NULL;
        refresh = true;

        clock_gettime(CLOCK_MONOTONIC, &start_time);

        main_loop = g_main_loop_new(NULL, FALSE);

        g_unix_signal_add(SIGINT, on_sigint, main_loop);

        if (song != NULL)
                emitStartPlayingMpris();
        else
                emitPlaybackStoppedMpris();

        g_timeout_add(100, mainloop_callback, NULL);
        g_main_loop_run(main_loop);
        g_main_loop_unref(main_loop);
}

void cleanupOnExit()
{
        pthread_mutex_lock(&dataSourceMutex);
        resetDecoders();
        resetVorbisDecoders();
        resetOpusDecoders();
        resetM4aDecoders();
        cleanupPlaybackDevice();
        cleanupAudioContext();
        emitPlaybackStoppedMpris();
        resetConsole();

        if (library == NULL || library->children == NULL)
        {
                printf("No Music found.\n");
                printf("Please make sure the path is set correctly. \n");
                printf("To set it type: kew path \"/path/to/Music\". \n");                
        }

        if (!userData.songdataADeleted)
        {
                userData.songdataADeleted = true;
                unloadSongData(&loadingdata.songdataA);
        }
        if (!userData.songdataBDeleted)
        {
                userData.songdataBDeleted = true;
                unloadSongData(&loadingdata.songdataB);
        }

        cleanupMpris();
        restoreTerminalMode();
        enableInputBuffering();
        setConfig(&settings);
        saveSpecialPlaylist(settings.path, playingMainPlaylist);
        freeAudioBuffer();
        deleteCache(tempCache);
        deleteTempDir();
        freeMainDirectoryTree();
        deletePlaylist(&playlist);
        deletePlaylist(originalPlaylist);
        deletePlaylist(specialPlaylist);
        free(specialPlaylist);
        free(originalPlaylist);
        setDefaultTextColor();
        showCursor();
        pthread_mutex_destroy(&(loadingdata.mutex));
        pthread_mutex_destroy(&(playlist.mutex));
        pthread_mutex_destroy(&(switchMutex));
        pthread_mutex_unlock(&dataSourceMutex);
        pthread_mutex_destroy(&(dataSourceMutex));

#ifdef DEBUG
        fclose(logFile);
#endif
        if (freopen("/dev/stderr", "w", stderr) == NULL)
        {
                perror("freopen error");
        }
}

void run()
{
        if (originalPlaylist == NULL)
        {
                originalPlaylist = malloc(sizeof(PlayList));
                *originalPlaylist = deepCopyPlayList(&playlist);
        }

        if (playlist.head == NULL)
        {
                appState.currentView = LIBRARY_VIEW;
        }

        initMpris();

        currentSong = playlist.head;
        play(currentSong);
        clearScreen();
        fflush(stdout);
}

void init()
{       
        disableInputBuffering();
        srand(time(NULL));
        initResize();
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &windowSize);
        enableScrolling();
        setNonblockingMode();
        tempCache = createCache();
        c_strcpy(loadingdata.filePath, sizeof(loadingdata.filePath), "");
        loadingdata.songdataA = NULL;
        loadingdata.songdataB = NULL;
        loadingdata.loadA = true;
        loadingdata.loadingFirstDecoder = true;
        audioData.restart = true;
        userData.songdataADeleted = true;
        userData.songdataBDeleted = true;
        initAudioBuffer();
        initVisuals();
        pthread_mutex_init(&dataSourceMutex, NULL);
        pthread_mutex_init(&switchMutex, NULL);
        pthread_mutex_init(&(loadingdata.mutex), NULL);
        pthread_mutex_init(&(playlist.mutex), NULL);
        nerdFontsEnabled = hasNerdFonts();
        createLibrary(&settings);
        fflush(stdout);        

#ifdef DEBUG
        g_setenv("G_MESSAGES_DEBUG", "all", TRUE);
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

void openLibrary()
{
        appState.currentView = LIBRARY_VIEW;
        init();
        playlist.head = NULL;
        run();
}

void playSpecialPlaylist()
{
        if (specialPlaylist->count == 0)
        {
                printf("Couldn't find any songs in the special playlist. Add a song by pressing '.' while it's playing. \n");
                exit(0);
        }
        
        playingMainPlaylist = true;

        init();
        deepCopyPlayListOntoList(specialPlaylist, &playlist);
        shufflePlaylist(&playlist);
        run();
}

void playAll()
{
        init();
        createPlayListFromFileSystemEntry(library, &playlist, MAX_FILES);
        if (playlist.count == 0)
        {
                exit(0);
        }
        shufflePlaylist(&playlist);
        run();
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

void handleOptions(int *argc, char *argv[])
{
        const char *noUiOption = "--noui";
        const char *noCoverOption = "--nocover";
        const char *quitOnStop = "--quitonstop";
        const char *quitOnStop2 = "-q";
        const char *exactOption = "--exact";
        const char *exactOption2 = "-e";

        int idx = -1;
        for (int i = 0; i < *argc; i++)
        {
                if (c_strcasestr(argv[i], noUiOption))
                {
                        uiEnabled = false;
                        idx = i;
                }
        }
        if (idx >= 0)
                removeArgElement(argv, idx, argc);

        idx = -1;
        for (int i = 0; i < *argc; i++)
        {
                if (c_strcasestr(argv[i], noCoverOption))
                {
                        coverEnabled = false;
                        idx = i;
                }
        }
        if (idx >= 0)
                removeArgElement(argv, idx, argc);

        idx = -1;
        for (int i = 0; i < *argc; i++)
        {
                if (c_strcasestr(argv[i], quitOnStop) || c_strcasestr(argv[i], quitOnStop2))
                {
                        quitAfterStopping = true;
                        idx = i;
                }
        }
        if (idx >= 0)
                removeArgElement(argv, idx, argc);

        idx = -1;
        for (int i = 0; i < *argc; i++)
        {
                if (c_strcasestr(argv[i], exactOption) || c_strcasestr(argv[i], exactOption2))
                {
                        exactSearch = true;
                        idx = i;
                }
        }
        if (idx >= 0)
                removeArgElement(argv, idx, argc);
}

int main(int argc, char *argv[])
{
        if ((argc == 2 && ((strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "-?") == 0))))
        {
                showHelp();
                exit(0);
        }
        else if (argc == 2 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0))
        {
                printAbout(NULL);
                exit(0);
        }

        moveConfigFiles(); // move from old location to new
        getConfig(&settings);

        if (argc == 3 && (strcmp(argv[1], "path") == 0))
        {
                c_strcpy(settings.path, sizeof(settings.path), argv[2]);
                setConfig(&settings);
                exit(0);
        }

        if (settings.path[0] == '\0')
        {
                printf("Please make sure the path is set correctly. \n");
                printf("To set it type: kew path \"/path/to/Music\". \n");
                exit(0);
        }

        atexit(cleanupOnExit);

        handleOptions(&argc, argv);
        loadSpecialPlaylist(settings.path);

        if (argc == 1)
        {
                openLibrary();
        }
        else if (argc == 2 && strcmp(argv[1], "all") == 0)
        {
                playAll();
        }
        else if (argc == 2 && strcmp(argv[1], ".") == 0)
        {
                playSpecialPlaylist();
        }
        else if (argc >= 2)
        {
                init();
                makePlaylist(argc, argv, exactSearch, settings.path);
                if (playlist.count == 0)
                        exit(0);
                run();
        }

        return 0;
}
