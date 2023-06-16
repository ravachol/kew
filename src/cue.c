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

//  \m/ keep on rocking in the free world \m/

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
#include "dir.h"
#include "settings.h"
#include "printfunc.h"
#include "playlist.h"
#include "events.h"
#include "file.h"
#include "visuals.h"
#include "albumart.h"
#include "player.h"
#include "arg.h"

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

#ifndef MAX_EVENTS_IN_QUEUE
#define MAX_EVENTS_IN_QUEUE 1
#endif
#define TRESHOLD_TERMINAL_SIZE 28

const char VERSION[] = "1.0.0";
char tagsFilePath[FILENAME_MAX];
bool repeatEnabled = false;
Node *currentSong;

struct Event processInput()
{
    struct Event event;
    event.type = EVENT_NONE;
    event.key = '\0';

    if (!isInputAvailable())
    {
        if (!isEventQueueEmpty())
            event = dequeueEvent();
        return event;
    }
    else
    {
        restoreCursorPosition();
    }
    char input = readInput();
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
    case 'v':
        event.type = EVENT_TOGGLEVISUALS;
        break;
    case 'b':
        event.type = EVENT_TOGGLEBLOCKS;
        break;
    case 'r':
        event.type = EVENT_TOGGLEREPEAT;
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
    default:
        break;
    }
    enqueueEvent(&event);

    if (!isEventQueueEmpty())
        event = dequeueEvent();

    return event;
}

void cleanup()
{
    clearRestOfScreen();
    cleanupPlaybackDevice();
    deleteFile(tagsFilePath);
}

int play(SongInfo song)
{
    char musicFilepath[MAX_FILENAME_LENGTH];
    strcpy(musicFilepath, song.filePath);
    int res = playSoundFile(musicFilepath);
    struct timespec start_time;
    double elapsedSeconds = 0.0;
    bool shouldQuit = false;
    int asciiHeight, asciiWidth;
    double duration = 0.0;
    if (song.duration > 0.0)
        duration = song.duration;
    else
        duration = getDuration(song.filePath);
    refresh = true;
    generateTempFilePath(tagsFilePath, "metatags", ".txt");
    extract_tags(strdup(musicFilepath), tagsFilePath);
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    setPlayListDurations(&playlist);

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

        elapsedSeconds = (double)(current_time.tv_sec - start_time.tv_sec) +
                         (double)(current_time.tv_nsec - start_time.tv_nsec) / 1e9;

        // Process events
        struct Event event = processInput();

        switch (event.type)
        {
        case EVENT_PLAY_PAUSE:
            pausePlayback();
            break;
        case EVENT_TOGGLEVISUALS:
            visualizationEnabled = !visualizationEnabled;
            strcpy(settings.visualizationEnabled, visualizationEnabled ? "1" : "0");
            restoreCursorPosition();
            clearRestOfScreen();
            break;
        case EVENT_TOGGLEREPEAT:
            repeatEnabled = !repeatEnabled;
            break;
        case EVENT_TOGGLECOVERS:
            coverEnabled = !coverEnabled;
            strcpy(settings.coverEnabled, coverEnabled ? "1" : "0");
            refresh = true;
            break;
        case EVENT_TOGGLEBLOCKS:
            coverBlocks = !coverBlocks;
            strcpy(settings.coverBlocks, coverBlocks ? "1" : "0");
            refresh = true;
            break;
        case EVENT_SHUFFLE:
            // Interrupt total playlist duration counting thread
            stopThread = 1;
            usleep(100000);
            stopThread = 0;
            playlist.totalDuration = 0.0;
            shufflePlaylist(&playlist);
            // And restart it
            setPlayListDurations(&playlist);
            break;
        case EVENT_QUIT:
            if (event.key == 'q' || elapsedSeconds > 2.0)
            {
                shouldQuit = true;
            }
            break;
        case EVENT_VOLUME_UP:
            adjustVolumePercent(5);
            break;
        case EVENT_VOLUME_DOWN:
            adjustVolumePercent(-5);
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
            if ((elapsedSeconds < duration) && !isPaused())
            {
                printPlayer(musicFilepath, tagsFilePath, elapsedSeconds, duration, &playlist);
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

int start()
{
    if (playlist.head == NULL)
        return -1;
    if (currentSong == NULL)
        currentSong = playlist.head;
    play(currentSong->song);
    restoreTerminalMode();
    free(g_audioBuffer);
    setConfig(&settings, SETTINGS_FILENAME);
    return 0;
}

void init()
{
    freopen("/dev/null", "w", stderr);
    signal(SIGWINCH, handleResize);
    initEventQueue();
    enableScrolling();
    setNonblockingMode();
    srand(time(NULL));
}

int main(int argc, char *argv[])
{
    getConfig();

    if (argc == 1 || (argc == 2 && ((strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "-?") == 0))))
    {
        printHelp();
    }
    else if (argc == 2 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0))
    {
        printVersion(VERSION);
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
        start();
    }
    return 0;
}