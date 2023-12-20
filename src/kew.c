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

#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <sys/param.h>
#include <fcntl.h>
#include <stdbool.h>
#include <time.h>
#include <poll.h>
#include <dirent.h>
#include <signal.h>
#include <unistd.h>
#include <glib.h>
#include <gio/gio.h>

#include "soundgapless.h"
#include "stringfunc.h"
#include "settings.h"
#include "cutils.h"
#include "playlist.h"
#include "events.h"
#include "file.h"
#include "visuals.h"
#include "albumart.h"
#include "player.h"
#include "cache.h"
#include "songloader.h"
#include "volume.h"
#include "playerops.h"
#include "mpris.h"
#include "soundcommon.h"

// #define DEBUG 1
#define MAX_SEQ_LEN 1024    // Maximum length of sequence buffer
#define MAX_TMP_SEQ_LEN 256 // Maximum length of temporary sequence buffer
#define COOLDOWN_MS 500
#define COOLDOWN2_MS 100
#define NUMKEYMAPPINGS = 30
FILE *logFile = NULL;
struct winsize windowSize;
static bool eventProcessed = false;
char digitsPressed[MAX_SEQ_LEN];
int digitsPressedCount = 0;
int maxDigitsPressedCount = 9;
bool gotoSong = false;
bool gPressed = false;

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

        if (songLoading || !loadedNextSong)
                return event;

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
                    strcmp(seq, settings.scrollDownAlt) == 0 || strcmp(seq, settings.seekBackward) == 0 || strcmp(seq, settings.seekForward) == 0)
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
                                      {settings.hardEndOfPlaylist, EVENT_GOTOENDOFPLAYLIST}};

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

        // Handle seek cooldown
        if (!cooldown2Elapsed && (event.type == EVENT_SEEKBACK || event.type == EVENT_SEEKFORWARD))
                event.type = EVENT_NONE;
        else if (event.type == EVENT_SEEKBACK || event.type == EVENT_SEEKFORWARD)
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

void prepareNextSong()
{
        while (!loadedNextSong && !loadingFailed)
        {
                c_sleep(10);
        }

        if (loadingFailed)
                return;

        if (!skipPrev && !gotoSong && !isRepeatEnabled())
        {
                if (nextSong != NULL)
                        currentSong = nextSong;
                else if (currentSong->next != NULL)
                {
                        currentSong = currentSong->next;
                }
                else
                {
                        quit();
                        return;
                }
        }
        else
        {
                skipPrev = false;
                gotoSong = false;
        }

        if (currentSong == NULL)
        {
                quit();
                return;
        }

        elapsedSeconds = 0.0;
        pauseSeconds = 0.0;
        totalPauseSeconds = 0.0;

        nextSong = NULL;

        refresh = true;

        if (!isRepeatEnabled())
        {
                pthread_mutex_lock(&(loadingdata.mutex));
                if (usingSongDataA &&
                    (skipping || (userData.currentSongData == NULL || userData.currentSongData->deleted ||
                                  (userData.currentSongData->trackId && loadingdata.songdataA &&
                                   strcmp(loadingdata.songdataA->trackId, userData.currentSongData->trackId) != 0))))
                {
                        unloadSongData(&loadingdata.songdataA);
                        userData.filenameA = NULL;
                        usingSongDataA = false;
                        loadedNextSong = false;
                }
                else if (!usingSongDataA &&
                         (skipping || (userData.currentSongData == NULL || userData.currentSongData->deleted ||
                                       (userData.currentSongData->trackId && loadingdata.songdataB &&
                                        strcmp(loadingdata.songdataB->trackId, userData.currentSongData->trackId) != 0))))
                {
                        unloadSongData(&loadingdata.songdataB);
                        userData.filenameB = NULL;
                        usingSongDataA = true;
                        loadedNextSong = false;
                }
                pthread_mutex_unlock(&(loadingdata.mutex));
        }

        clock_gettime(CLOCK_MONOTONIC, &start_time);
}

void refreshPlayer()
{
        SongData *songData = (audioData.currentFileIndex == 0) ? userData.songdataA : userData.songdataB;

        if (!isEOFReached() && !isImplSwitchReached() && songData != NULL && !songData->deleted)
        {
                if (refresh)
                {
                        // update mpris
                        emitMetadataChanged(
                            songData->metadata->title,
                            songData->metadata->artist,
                            songData->metadata->album,
                            songData->coverArtPath,
                            songData->trackId != NULL ? songData->trackId : "");
                }

                printPlayer(songData, elapsedSeconds, &playlist);
        }
}

void handleGoToSong()
{
        if (digitsPressedCount == 0)
        {
                skipToNumberedSong(chosenSong + 1);
        }
        else
        {
                resetPlaylistDisplay = true;
                int songNumber = atoi(digitsPressed);
                memset(digitsPressed, '\0', sizeof(digitsPressed));
                digitsPressedCount = 0;
                skipToNumberedSong(songNumber);
        }        
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
                toggleVisualizer();
                break;
        case EVENT_TOGGLEREPEAT:
                toggleRepeat();
                break;
        case EVENT_TOGGLECOVERS:
                toggleCovers();
                break;
        case EVENT_TOGGLEBLOCKS:
                toggleBlocks();
                break;
        case EVENT_SHUFFLE:
                toggleShuffle();
                break;
        case EVENT_TOGGLE_PROFILE_COLORS:
                toggleColors();
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
                addToPlaylist();
                break;
        case EVENT_DELETEFROMMAINPLAYLIST:
                // FIXME implement this
                break;
        case EVENT_EXPORTPLAYLIST:
                savePlaylist();
                break;
        case EVENT_SHOWKEYBINDINGS:
                toggleShowKeyBindings();
                break;
        case EVENT_SHOWINFO:
                toggleShowPlaylist();
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
        setCursorPosition(1, 1);
        clearRestOfScreen();
        while (resizeFlag)
        {
                resizeFlag = 0;
                c_sleep(100);
        }
        alarm(0); // Cancel timer
        refresh = true;
        printf("\033[1;1H");
        clearRestOfScreen();
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
        if (nextSong == NULL && !songLoading)
        {
                tryNextSong = currentSong->next;
                songLoading = true;
                loadingdata.loadA = !usingSongDataA;
                loadNext(&loadingdata);
        }
}

void tryLoadNext()
{
        songHasErrors = false;
        clearingErrors = true;
        if (tryNextSong == NULL)
                tryNextSong = currentSong->next;
        else
                tryNextSong = tryNextSong->next;

        if (tryNextSong != NULL)
        {
                songLoading = true;
                loadingdata.loadA = !usingSongDataA;
                loadSong(tryNextSong, &loadingdata);
        }
        else
        {
                clearingErrors = false;
        }
}

gboolean mainloop_callback(gpointer data)
{
        calcElapsedTime();
        handleInput();

        // Process GDBus events in the global_main_context
        while (g_main_context_pending(global_main_context))
        {
                g_main_context_iteration(global_main_context, FALSE);
        }

        updatePlayer();

        if (!loadedNextSong)
                loadAudioData();

        if (songHasErrors)
                tryLoadNext();

        if (isPlaybackDone())
        {
                updateLastSongSwitchTime();
                prepareNextSong();
                if (!doQuit)
                        switchAudioImplementation();
        }

        if (doQuit || loadingFailed)
        {
                g_main_loop_quit(main_loop);
                return FALSE;
        }

        return TRUE;
}

void play(Node *song)
{
        updateLastInputTime();
        updateLastSongSwitchTime();
        pthread_mutex_init(&(loadingdata.mutex), NULL);

        loadFirst(song);

        if (songHasErrors)
        {
                printf("Couldn't play any of the songs.\n");
                return;
        }

        userData.currentPCMFrame = 0;
        userData.filenameA = loadingdata.songdataA->pcmFilePath;
        userData.songdataA = loadingdata.songdataA;
        userData.currentSongData = userData.songdataA;

        createAudioDevice(&userData);

        loadedNextSong = false;
        nextSong = NULL;
        refresh = true;

        clock_gettime(CLOCK_MONOTONIC, &start_time);
        calculatePlayListDuration(&playlist);

        main_loop = g_main_loop_new(NULL, FALSE);

        emitStartPlayingMpris();

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
        cleanupPlaybackDevice();
        cleanupAudioContext();
        emitPlaybackStoppedMpris();
        unloadSongData(&loadingdata.songdataA);
        unloadSongData(&loadingdata.songdataB);
        stopFFmpeg();
        cleanupMpris();
        restoreTerminalMode();
        enableInputBuffering();
        setConfig();
        saveMainPlaylist(settings.path, playingMainPlaylist);
        freeAudioBuffer();
        deleteCache(tempCache);
        deleteTempDir();
        deletePlaylist(&playlist);
        deletePlaylist(mainPlaylist);
        deletePlaylist(originalPlaylist);
        free(mainPlaylist);
        freeVisuals();
        showCursor();
        pthread_mutex_unlock(&dataSourceMutex);

#ifdef DEBUG
        fclose(logFile);
#endif
        if (freopen("/dev/stderr", "w", stderr) == NULL) {
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
                showCursor();
                restoreTerminalMode();
                enableInputBuffering();
                return;
        }

        initMpris();

        currentSong = playlist.head;
        play(currentSong);
        clearScreen();
        fflush(stdout);
}

void init()
{
        clearScreen();
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
        initAudioBuffer();
        initVisuals();
        pthread_mutex_init(&dataSourceMutex, NULL);

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

void playMainPlaylist()
{
        if (mainPlaylist->count == 0)
        {
                printf("Couldn't find any songs in the main playlist. Add a song by pressing '.' while it's playing. \n");
                exit(0);
        }
        init();
        playingMainPlaylist = true;
        playlist = deepCopyPlayList(mainPlaylist);
        shufflePlaylist(&playlist);
        run();
}

void playAll(int argc, char **argv)
{
        init();
        makePlaylist(argc, argv);
        if (playlist.count == 0)
        {
                printf("Please make sure the path is set correctly. \n");
                printf("To set it type: kew path \"/path/to/Music\". \n");
                exit(0);
        }
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
}

int main(int argc, char *argv[])
{
        atexit(cleanupOnExit);
        getConfig();
        loadMainPlaylist(settings.path);

        handleOptions(&argc, argv);

        if (argc == 1)
        {
                playAll(argc, argv);
        }
        else if (strcmp(argv[1], ".") == 0)
        {
                playMainPlaylist();
        }
        else if ((argc == 2 && ((strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "-?") == 0))))
        {
                showHelp();
        }
        else if (argc == 2 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0))
        {
                printAbout();
        }
        else if (argc == 3 && (strcmp(argv[1], "path") == 0))
        {
                c_strcpy(settings.path, sizeof(settings.path), argv[2]);
                setConfig();
        }
        else if (argc >= 2)
        {
                init();
                makePlaylist(argc, argv);
                run();
        }

        return 0;
}
