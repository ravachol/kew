// cue - command-line music player
// Copyright (C) 2022 Ravachol
//
// http://github.com/ravachol/cue
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version, provided that the above
// copyright notice and this permission notice appear in all copies.

//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.

#include <stdio.h>
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
#include "sound.h"
#include "stringfunc.h"
#include "settings.h"
#include "printfunc.h"
#include "playlist.h"
#include "events.h"
#include "file.h"
#include "visuals.h"
#include "albumart.h"
#include "player.h"
#include "arg.h"
#include "cache.h"

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

#ifndef MAX_EVENTS_IN_QUEUE
#define MAX_EVENTS_IN_QUEUE 1
#endif
#define TRESHOLD_TERMINAL_SIZE 28

bool repeatEnabled = false;
bool playingMainPlaylist = false;
bool shouldQuit = false;

static int volumeUpCooldown = 0;
static int volumeDownCooldown = 0;
struct Event lastEvent;

struct Event processInput()
{
    struct Event event;
    event.type = EVENT_NONE;
    event.key = '\0';

    if (!isInputAvailable())
    {
        saveCursorPosition();
    }
    else
    {
        restoreCursorPosition();
    }
    
    char input = readInput();

    while (isInputAvailable())
    {
        input = readInput();
    }
    event.type = EVENT_KEY_PRESS;
    event.key = input;

    switch (event.key)
    {
    case 'q':
        event.type = EVENT_QUIT;
        break;
    case 's':
        event.type = EVENT_SHUFFLE;
        break;
    case 'c':
        event.type = EVENT_TOGGLECOVERS;
        break;
    case 'e':
        event.type = EVENT_TOGGLEEQUALIZER;
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
    case 'p':
        event.type = EVENT_EXPORTPLAYLIST;
        break;
    case 'A': // Up arrow
        event.type = EVENT_VOLUME_UP;
        break;
    case 'B': // Down arrow
        event.type = EVENT_VOLUME_DOWN;
        break;
    case 'C': // Right arrow
        event.type = EVENT_NEXT;
        break;
    case 'D': // Left arrow
        event.type = EVENT_PREV;
        break;
    case ' ':
        event.type = EVENT_PLAY_PAUSE;
        break;
    case 'P': // F1
        refresh = true;
        printInfo = !printInfo;
        break;
    default:
        break;
    }
    return event;
}

void cleanup()
{
    clearRestOfScreen();
    coverArtFilePath[0] = '\0';
    deleteCachedFiles(tempCache);
}

void doShuffle()
{
    stopPlayListDurationCount();
    usleep(100000);
    playlist.totalDuration = 0.0;
    shufflePlaylist(&playlist);
    calculatePlayListDuration(&playlist);    
}

void addToPlaylist()
{
    if (!playingMainPlaylist)
    {
        addToList(mainPlaylist, currentSong->song);
    }
}

void toggleBlocks()
{
    coverBlocks = !coverBlocks;
    strcpy(settings.coverBlocks, coverBlocks ? "1" : "0");
    if (coverEnabled)
        refresh = true;
}

void toggleCovers()
{
    coverEnabled = !coverEnabled;
    strcpy(settings.coverEnabled, coverEnabled ? "1" : "0");
    refresh = true;    
}

void toggleEqualizer()
{
    equalizerEnabled = !equalizerEnabled;
    strcpy(settings.equalizerEnabled, equalizerEnabled ? "1" : "0");
    restoreCursorPosition();
    refresh = true;
}

void togglePause(double *totalPauseSeconds, double pauseSeconds, struct timespec *pause_time)
{
    pausePlayback();
    if (isPaused())
        clock_gettime(CLOCK_MONOTONIC, pause_time);
    else
        *totalPauseSeconds += pauseSeconds;    
}

void quit()
{
    shouldQuit = true;
}

int play(SongInfo song)
{
    cleanup();
    if (g_audioBuffer != NULL)
    {
        stopPlayback();
        free(g_audioBuffer);
        g_audioBuffer = NULL;
    }
    char musicFilepath[MAX_FILENAME_LENGTH];
    strcpy(musicFilepath, song.filePath);
    extractTags(musicFilepath, &metadata);
    int res = playSoundFile(musicFilepath);
    struct timespec start_time;
    struct timespec pause_time;
    double elapsedSeconds = 0.0;
    double pauseSeconds = 0.0;
    double totalPauseSeconds = 0.0;
    int asciiHeight, asciiWidth;
    double duration = 0.0;
    if (song.duration > 0.0)
        duration = song.duration;
    else
        duration = getDuration(song.filePath);
    refresh = true;

    clock_gettime(CLOCK_MONOTONIC, &start_time);
    calculatePlayListDuration(&playlist);

    if (res != 0)
    {
        cleanup();
        currentSong = getListNext(currentSong);
        if (currentSong != NULL)
            return play(currentSong->song);
    }

    while (true)
    {
        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);

        if (!isPaused())
        {
            elapsedSeconds = (double)(current_time.tv_sec - start_time.tv_sec) +
                             (double)(current_time.tv_nsec - start_time.tv_nsec) / 1e9;
            elapsedSeconds -= totalPauseSeconds;
        }
        else
        {
            pauseSeconds = (double)(current_time.tv_sec - pause_time.tv_sec) +
                           (double)(current_time.tv_nsec - pause_time.tv_nsec) / 1e9;
        }
        // Process events
        struct Event event = processInput();

        switch (event.type)
        {
        case EVENT_PLAY_PAUSE:
            togglePause(&totalPauseSeconds, pauseSeconds, &pause_time);
            break;
        case EVENT_TOGGLEEQUALIZER:
            toggleEqualizer();
            break;
        case EVENT_TOGGLEREPEAT:
            repeatEnabled = !repeatEnabled;
            break;
        case EVENT_TOGGLECOVERS:
            toggleCovers();
            break;
        case EVENT_TOGGLEBLOCKS:
            toggleBlocks();
            break;
        case EVENT_SHUFFLE:
            doShuffle();            
            break;
        case EVENT_QUIT:
            quit();
            break;
        case EVENT_VOLUME_UP:
            adjustVolumePercent(2);
            break;
        case EVENT_VOLUME_DOWN:
            adjustVolumePercent(-2);
            break;
        case EVENT_NEXT:
            if (currentSong->next != NULL)
            {
                cleanup();
                currentSong = getListNext(currentSong);
                return play(currentSong->song);
            }
            break;
        case EVENT_PREV:
            if (currentSong->prev != NULL)
            {
                cleanup();
                currentSong = getListPrev(currentSong);
                return play(currentSong->song);
            }    
            break;
        case EVENT_ADDTOMAINPLAYLIST:
            addToPlaylist();
            break;
        case EVENT_DELETEFROMMAINPLAYLIST:
            if (playingMainPlaylist)
            {
                currentSong = deleteFromList(&playlist, currentSong);
                cleanup();
                if (currentSong != NULL)
                    return play(currentSong->song);
                else
                    return 0;
            }
            break;
        case EVENT_EXPORTPLAYLIST:
            savePlaylist();
            break;
        default:
            break;
        }

        signal(SIGWINCH, handleResize);

        struct sigaction sa;
        sa.sa_handler = resetResizeFlag;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGALRM, &sa, NULL);

        if (resizeFlag)
        {
            alarm(1); // Timer
            setCursorPosition(1, 1);
             clearRestOfScreen();
            while (resizeFlag)
            {
                usleep(100000);
            }
            alarm(0); // Cancel timer
            refresh = true;
            printf("\033[1;1H");
            clearRestOfScreen();
        }
        else
        {
            if (elapsedSeconds < duration)
            {
                printPlayer(musicFilepath, &metadata, elapsedSeconds, duration, &playlist);
            }
        }

        if (shouldQuit)
        {
            break;
        }
        if (isPlaybackDone())
        {
            cleanup();
            if (!repeatEnabled)
                currentSong = getListNext(currentSong);
            if (currentSong != NULL)
                play(currentSong->song);
            break;
        }
        usleep(100000);
    }
    cleanup();
    return 0;
}

int run()
{
    if (playlist.head == NULL)
    {
        showCursor();
        restoreTerminalMode();
        enableInputBuffering();
        return -1;        
    }   
    if (currentSong == NULL)
        currentSong = playlist.head;
    play(currentSong->song);
    restoreTerminalMode();
    enableInputBuffering();    
    if (g_audioBuffer != NULL)
    {
        free(g_audioBuffer);
        g_audioBuffer = NULL;
    }
    setConfig();
    saveMainPlaylist(settings.path, playingMainPlaylist);
    free(mainPlaylist);
    cleanupPlaybackDevice();
    deleteCachedFiles(tempCache);
    deleteCache(tempCache);
    deleteTempDir();
    showCursor();
    printf("\n");
   
    return 0;
}

void init()
{
    disableInputBuffering();
    freopen("/dev/null", "w", stderr);
    signal(SIGWINCH, handleResize);
    enableScrolling();
    setNonblockingMode();
    srand(time(NULL));
    tempCache = createCache();
}

void playMainPlaylist()
{
    if (mainPlaylist->count == 0)
    {
        showHelp();
    }
    playingMainPlaylist = true;
    playlist = deepCopyPlayList(mainPlaylist);
    shufflePlaylist(&playlist);
    init();
    run();
}

void playAll(int argc, char **argv)
{
    init();
    makePlaylist(argc, argv);
    run();
}

int main(int argc, char *argv[])
{
    getConfig();
    loadMainPlaylist(settings.path);

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
        printAsciiLogo();
        showVersion();
    }
    else if (argc == 3 && (strcmp(argv[1], "path") == 0))
    {
        strcpy(settings.path, argv[2]);
        setConfig();
    }
    else if (argc >= 2)
    {
        init();
        handleSwitches(&argc, argv);
        makePlaylist(argc, argv);
        run();
    }
    return 0;
}