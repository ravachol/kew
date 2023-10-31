/* cue - a command-line music player
Copyright (C) 2022 Ravachol

http://github.com/ravachol/cue

 $$$$$$$\ $$\   $$\  $$$$$$\
$$  _____|$$ |  $$ |$$  __$$\
$$ /      $$ |  $$ |$$$$$$$$ |
$$ |      $$ |  $$ |$$   ____|
\$$$$$$$\ \$$$$$$  |\$$$$$$$\
 \_______| \______/  \_______|

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
#include <bits/sigaction.h>
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

// #define DEBUG 1
FILE *logFile = NULL;
struct winsize windowSize;

#define COOLDOWN_DURATION 1000

static bool eventProcessed = false;

bool gotoSong = false;
#define MAX_SEQ_LEN 1024    // Maximum length of sequence buffer
#define MAX_TMP_SEQ_LEN 256 // Maximum length of temporary sequence buffer
char digitsPressed[MAX_SEQ_LEN];
int digitsPressedCount = 0;
int maxDigitsPressedCount = 9;
bool gPressed = false;

bool isCooldownElapsed()
{
        struct timespec currentTime;
        clock_gettime(CLOCK_MONOTONIC, &currentTime);
        double elapsedMilliseconds = (currentTime.tv_sec - lastInputTime.tv_sec) * 1000.0 +
                                     (currentTime.tv_nsec - lastInputTime.tv_nsec) / 1000000.0;
        return elapsedMilliseconds >= COOLDOWN_DURATION;
}

struct Event processInput()
{
        struct Event event;
        event.type = EVENT_NONE;
        event.key = '\0';
        bool cooldownElapsed = false;

        if (songLoading || !loadedNextSong)
                return event;

        if (!isInputAvailable())
                return event;

        if (isCooldownElapsed() && !eventProcessed)
                cooldownElapsed = true;

        int seqLength = 0;
        char seq[MAX_SEQ_LEN];
        seq[0] = '\0'; // Set initial value
        int keyReleased = 0;

        while (isInputAvailable())
        {
                char tmpSeq[MAX_TMP_SEQ_LEN];

                seqLength = seqLength + readInputSequence(tmpSeq, sizeof(tmpSeq));

                if (seqLength <= 0)
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

                if (strcmp(seq, "[A") == 0 || strcmp(seq, "[B") == 0 || strcmp(seq, "k") == 0 || strcmp(seq, "j") == 0)
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

        if (seqLength > 1)
        { // 0x1B +
                if (strcmp(seq, "[A") == 0)
                {
                        // Arrow Up
                        if (printInfo)
                                event.type = EVENT_SCROLLPREV;
                }
                else if (strcmp(seq, "[B") == 0)
                {
                        // Arrow Down
                        if (printInfo)
                                event.type = EVENT_SCROLLNEXT;
                }
                else if (strcmp(seq, "[C") == 0)
                {
                        // Arrow Left
                        event.type = EVENT_NEXT;
                }
                else if (strcmp(seq, "[D") == 0)
                {
                        // Arrow Right
                        event.type = EVENT_PREV;
                }
                else if (strcmp(seq, "OQ") == 0 || strcmp(seq, "[[B") == 0)
                {
                        // F2 key
                        event.type = EVENT_SHOWINFO;
                }
                else if (strcmp(seq, "OR") == 0 || strcmp(seq, "[[C") == 0)
                {
                        // F3 key
                        event.type = EVENT_SHOWKEYBINDINGS;
                }
                else
                {
                        for (int i = 0; i < MAX_SEQ_LEN; i++)
                        {
                                if (isdigit(seq[i]))
                                {
                                        digitsPressed[digitsPressedCount++] = seq[i];
                                }
                                else
                                {
                                        if (seq[i] == '\0')
                                                break;

                                        if (seq[i] != 'G' && seq[i] != 'g' && seq[i] != '\n')
                                        {
                                                memset(digitsPressed, '\0', sizeof(digitsPressed));
                                                digitsPressedCount = 0;
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
        }
        else if (isdigit(event.key))
        {
                if (digitsPressedCount < maxDigitsPressedCount)
                        digitsPressed[digitsPressedCount++] = event.key;
        }
        else
        {
                switch (event.key)
                {
                case 'G':
                        if (digitsPressedCount > 0)
                        {
                                event.type = EVENT_GOTOSONG;
                        }
                        else
                        {
                                event.type = EVENT_GOTOENDOFPLAYLIST;
                        }
                        break;
                case 'g':
                        if (digitsPressedCount > 0)
                        {
                                event.type = EVENT_GOTOSONG;
                        }
                        else if (gPressed)
                        {
                                event.type = EVENT_GOTOBEGINNINGOFPLAYLIST;
                                gPressed = false;
                        }
                        else
                        {
                                gPressed = true;
                        }
                        break;
                case '\n':
                        if (digitsPressedCount > 0 || printInfo)
                        {
                                event.type = EVENT_GOTOSONG;
                        }
                        break;
                case 'k': // Next song
                        if (printInfo)
                                event.type = EVENT_SCROLLPREV;
                        break;
                case 'j': // Prev song
                        if (printInfo)
                                event.type = EVENT_SCROLLNEXT;
                        break;
                case 'l':
                        event.type = EVENT_NEXT;
                        break;
                case 'h':
                        event.type = EVENT_PREV;
                        break;
                case '+': // Volume UP
                        event.type = EVENT_VOLUME_UP;
                        break;
                case '-': // Volume DOWN
                        event.type = EVENT_VOLUME_DOWN;
                        break;
                case 'p': // Play/Pau se
                        event.type = EVENT_PLAY_PAUSE;
                        break;
                case 'q':
                        event.type = EVENT_QUIT;
                        break;
                case 's':
                        event.type = EVENT_SHUFFLE;
                        break;
                case 'c':
                        event.type = EVENT_TOGGLECOVERS;
                        break;
                case 'v':
                        event.type = EVENT_TOGGLEVISUALIZER;
                        break;
                case 'b':
                        event.type = EVENT_TOGGLEBLOCKS;
                        break;
                case 'a':
                        event.type = EVENT_ADDTOMAINPLAYLIST;
                        break;
                case 'd':
                        event.type = EVENT_DELETEFROMMAINPLAYLIST;
                        break;
                case 'r':
                        event.type = EVENT_TOGGLEREPEAT;
                        break;
                case 'x':
                        event.type = EVENT_EXPORTPLAYLIST;
                        break;
                case 'i':
                        event.type = EVENT_TOGGLE_PROFILE_COLORS;
                        break;
                case ' ':
                        event.type = EVENT_PLAY_PAUSE;
                        break;
                default:
                        break;
                }
        }

        // cooldown is for next and previous only
        if (!cooldownElapsed && (event.type == EVENT_NEXT || event.type == EVENT_PREV))
                event.type = EVENT_NONE;

        if (event.type != EVENT_GOTOSONG && event.type != EVENT_NONE)
        {
                memset(digitsPressed, '\0', sizeof(digitsPressed));
                digitsPressedCount = 0;
        }

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

        if (!skipPrev && !gotoSong && !repeatEnabled)
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

        if (!repeatEnabled)
        {
                pthread_mutex_lock(&(loadingdata.mutex));
                if (usingSongDataA && 
                        (skipping || (userData.currentSongData == NULL ||
                        (userData.currentSongData->trackId && loadingdata.songdataA && 
                        strcmp(loadingdata.songdataA->trackId, userData.currentSongData->trackId) != 0))))
                {
                        unloadSongData(&loadingdata.songdataA);
                        userData.filenameA = NULL;
                        usingSongDataA = false;
                        loadedNextSong = false;
                }
                else if (!usingSongDataA && 
                        (skipping || (userData.currentSongData == NULL ||
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
        SongData *songData = usingSongDataA ? loadingdata.songdataA : loadingdata.songdataB;

        // Check if we have the correct one
        if (userData.currentSongData != NULL && 
                userData.currentSongData->trackId != NULL && 
                songData != NULL && strcmp(songData->trackId, userData.currentSongData->trackId) != 0)
        {                
                if (usingSongDataA)
                {
                        // Try the other one
                        if (loadingdata.songdataB != NULL && strcmp(loadingdata.songdataB->trackId, userData.currentSongData->trackId) == 0)
                        {
                                songData = loadingdata.songdataB;
                        }
                }
                else
                {
                        if (loadingdata.songdataA != NULL && strcmp(loadingdata.songdataA->trackId, userData.currentSongData->trackId) == 0)
                        {
                                songData = loadingdata.songdataA;
                        }
                }
        }

        if (songData != NULL)
        {
                if (refresh)
                {
                        // update mpris
                        emitMetadataChanged(
                            songData->metadata->title ? songData->metadata->title : "",
                            songData->metadata->artist ? songData->metadata->artist : "",
                            songData->metadata->album ? songData->metadata->album : "",
                            songData->coverArtPath ? songData->coverArtPath : "",
                            songData->trackId ? songData->trackId : "");
                }

                printPlayer(songData, elapsedSeconds, &playlist);
        }        
}

void handleGotoSong()
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

void handleInput()
{
        struct Event event = processInput();

        switch (event.type)
        {
        case EVENT_GOTOBEGINNINGOFPLAYLIST:
                skipToNumberedSong(1);
                break;
        case EVENT_GOTOENDOFPLAYLIST:
                skipToLastSong();
                break;
        case EVENT_GOTOSONG:
                handleGotoSong();
                break;
        case EVENT_PLAY_PAUSE:
                togglePause(&totalPauseSeconds, pauseSeconds, &pause_time);
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
                skipToNextSong();
                break;
        case EVENT_PREV:
                skipToPrevSong();
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
        cleanupPlaybackDevice();
        emitPlaybackStoppedMpris();
        unloadSongData(&loadingdata.songdataA);
        unloadSongData(&loadingdata.songdataB);
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
        showCursor();

#ifdef DEBUG
        fclose(logFile);
#endif
        stderr = fdopen(STDERR_FILENO, "w");
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
        srand(time(NULL));
        tempCache = createCache();
        c_strcpy(loadingdata.filePath, sizeof(loadingdata.filePath), "");
        loadingdata.songdataA = NULL;
        loadingdata.songdataB = NULL;
        loadingdata.loadA = true;

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
                printf("Couldn't find any songs in the main playlist. Add a song by pressing 'a' while it's playing. \n");
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
                printf("To set it type: cue path \"/path/to/Music\". \n");
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