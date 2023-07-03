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
#include "soundgapless.h"
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
#include "songloader.h"

#ifndef SONG_BUFFER_SIZE
#define SONG_BUFFER_SIZE 1024 * 1024
#endif
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

#ifndef MAX_EVENTS_IN_QUEUE
#define MAX_EVENTS_IN_QUEUE 1
#endif
#define TRESHOLD_TERMINAL_SIZE 28

typedef struct
{
	char filePath[MAXPATHLEN];
    SongData *songdataA;
    SongData *songdataB;
    bool loadA;
    pthread_mutex_t mutex;
} LoadingThreadData;

bool playingMainPlaylist = false;
bool shouldQuit = false;
bool usingSongDataA = true;
bool firstCall = true;
bool assignedToUserdata = false;
bool loadingFailed = false;
bool skipPrev = false;
bool skipping = false;

struct timespec current_time;
struct timespec start_time;
struct timespec pause_time;

double elapsedSeconds = 0.0;
double pauseSeconds = 0.0;
double totalPauseSeconds = 0.0;

volatile bool loadedSong = false;

UserData userData;

LoadingThreadData loadingdata;

Node *nextSong = NULL;
Node *prevSong = NULL;

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
    
    char input = '\0';

    while (isInputAvailable() == 1)
    {
       input = readInput();
    }
    event.type = EVENT_NONE;
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
    cleanupPlaybackDevice();    
    unloadSongData(loadingdata.songdataA);
    unloadSongData(loadingdata.songdataB);
    clearRestOfScreen();
}

void doShuffle()
{
    stopPlayListDurationCount();
    usleep(100000);
    playlist.totalDuration = 0.0;
    shufflePlaylist(&playlist);
    calculatePlayListDuration(&playlist);
    usleep(100000);    
    loadedSong = false;
    refresh = true;
    nextSong = NULL;       
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

void toggleRepeat()
{
    repeatEnabled = !repeatEnabled;    
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

void freeAudioBuffer()
{
    if (g_audioBuffer != NULL)
    {
        free(g_audioBuffer);
        g_audioBuffer = NULL;
    }    
}

void resize()
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

void loadPcmFileIntoMemory(const char* inputFilePath, long seek, char** filePart, long* fileSize, long* nextFile)
{
    FILE* file = fopen(inputFilePath, "rb");
    if (file == NULL)
    {
        //printf("Failed to open PCM file: %s\n", inputFilePath);
        return;
    }

    // Determine the file size if not already determined
    if (*fileSize == 0)
    {
        fseek(file, 0, SEEK_END);
        *fileSize = ftell(file);
        rewind(file);
    }

    // Set the file position to the seek position
    fseek(file, seek, SEEK_SET);

    // Calculate the remaining bytes to read
    long remainingBytes = *fileSize - seek;
    long bytesToRead = (remainingBytes < SONG_BUFFER_SIZE) ? remainingBytes : SONG_BUFFER_SIZE;

    // Reallocate memory for the combined file part
    *filePart = (char*)realloc(*filePart, *nextFile + bytesToRead);
    if (*filePart == NULL)
    {
        printf("Failed to reallocate memory for the file part.\n");
        fclose(file);
        return;
    }

    // Read the file part and append it to the existing buffer
    size_t bytesRead = fread(*filePart + *nextFile, 1, bytesToRead, file);

    // Update the nextFile position
    *nextFile += bytesRead;

    fclose(file);
}

void* songDataReaderThread(void* arg)
{
    LoadingThreadData* loadingdata = (LoadingThreadData*)arg;

    // Acquire the mutex lock
    pthread_mutex_lock(&(loadingdata->mutex));

    char* filePart = NULL;
    long fileSize = 0;
    long nextFile = 0;  

    char filepath[MAXPATHLEN];
    strcpy(filepath, loadingdata->filePath);
    SongData* songdata = NULL;
    songdata = loadSongData(filepath);

    while (true)
    {
        usleep(10000);
        if (songdata->pcmFilePath[0] != '\0')
                loadPcmFileIntoMemory(songdata->pcmFilePath, nextFile, &filePart, &fileSize, &nextFile);        

        if (nextFile == fileSize)
        {
            // The entire file has been loaded into filePart
            songdata->pcmFile = filePart;
            songdata->pcmFileSize = fileSize;
            if (loadingdata->loadA)
                loadingdata->songdataA = songdata;
            else
                loadingdata->songdataB = songdata;
            // Reset for the next iteration
            loadedSong = true;
            break;
        }                
    }       
    fileSize = 0;
    nextFile = 0;

    // Release the mutex lock
    pthread_mutex_unlock(&(loadingdata->mutex));

    return NULL;
}

void loadSong(Node *song, LoadingThreadData* loadingdata)
{
    if (song == NULL) 
    {
        loadingFailed = true;
        return;
    }

    strcpy(loadingdata->filePath, song->song.filePath);

    pthread_t loadingThread;
    pthread_create(&loadingThread, NULL, songDataReaderThread, (void*)loadingdata);
}

void loadNext(LoadingThreadData* loadingdata)
{
    nextSong = getListNext(currentSong);

    if (nextSong == NULL) 
    {
        loadedSong = true;
        return;
    }
    strcpy(loadingdata->filePath, nextSong->song.filePath);

    pthread_t loadingThread;
    pthread_create(&loadingThread, NULL, songDataReaderThread, (void*)loadingdata);
}

void loadPrev(LoadingThreadData* loadingdata)
{
    prevSong = getListPrev(currentSong);

    if (prevSong == NULL) return;

    strcpy(loadingdata->filePath, prevSong->song.filePath);

    pthread_t loadingThread;
    pthread_create(&loadingThread, NULL, songDataReaderThread, (void*)loadingdata);
}

bool isPlaybackOfListDone()
{
    return (userData.endOfListReached == 1);
}

void assignUserData()
{
    if (usingSongDataA)
    {
        userData.pcmFileB.filename = loadingdata.songdataB->filePath;
        userData.pcmFileB.pcmData = loadingdata.songdataB->pcmFile;
        userData.pcmFileB.pcmDataSize = loadingdata.songdataB->pcmFileSize;                
    }
    else
    {
        userData.pcmFileA.filename = loadingdata.songdataA->filePath;
        userData.pcmFileA.pcmData = loadingdata.songdataA->pcmFile;
        userData.pcmFileA.pcmDataSize = loadingdata.songdataA->pcmFileSize;                
    }   
    assignedToUserdata = true;    
}

void prepareNextSong()
{
    if (!skipPrev && !repeatEnabled)
        currentSong = currentSong->next;
    else
        skipPrev = false;

    if (currentSong == NULL)
    {
        quit();
        return;
    }

    elapsedSeconds = 0.0;
    pauseSeconds = 0.0;
    totalPauseSeconds = 0.0;

    loadedSong = false;
    nextSong = NULL;

    refresh = true;

    if (!repeatEnabled)
    {
        if (usingSongDataA)
        {
            userData.pcmFileA.filename = NULL;
            if (userData.pcmFileA.pcmData != NULL)
            {
                free(userData.pcmFileA.pcmData);
                userData.pcmFileA.pcmData = NULL;
            }
            userData.pcmFileA.pcmData = NULL;
            unloadSongData(loadingdata.songdataA);
        }
        else {
            userData.pcmFileB.filename = NULL;
            if (userData.pcmFileB.pcmData != NULL)
            {
                free(userData.pcmFileB.pcmData);
                userData.pcmFileB.pcmData = NULL;
            }
            userData.pcmFileB.pcmData = NULL;
            unloadSongData(loadingdata.songdataB);
        }

        usingSongDataA = !usingSongDataA; 
    }
    skipping = false;

    clock_gettime(CLOCK_MONOTONIC, &start_time);
}

void skipToNextSong()
{
    if (currentSong->next == NULL)
    {
        return;
    }    
    if (skipping)
        return;            
    skipping = true;    
    while (!loadedSong && !loadingFailed)
    {
        usleep(10000);
    }
    if (!assignedToUserdata)
        assignUserData();
    skip();
}

void skipToPrevSong()
{
    if (currentSong->prev == NULL)
    {
        return;
    }    
    if (skipping)
        return;
    skipping = true;
    while ((!loadedSong && !loadingFailed) && currentSong->next != NULL)
    {
        usleep(1000);
    }
    if (!assignedToUserdata)
        assignUserData();    
    if (currentSong->prev != NULL)
    {
        currentSong = currentSong->prev;
    }

    skipPrev = true;

    loadedSong = false;
    bool loading = false;

    while (!loadedSong && !loadingFailed)
    {       
        if (!loadedSong)
        {
            assignedToUserdata = false;
            if (loading == false)
            {
                loading = true;
                if (usingSongDataA)
                    loadingdata.loadA = false;
                else
                    loadingdata.loadA = true;
                
                loadSong(currentSong, &loadingdata);
            }               
        }

        usleep(10000);
    }
    if (!assignedToUserdata)
        assignUserData();
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    skip();
}

void calcElapsedTime()
{
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
}

void refreshPlayer()
{    
    if (usingSongDataA)
        printPlayer(loadingdata.songdataA, elapsedSeconds, &playlist);
    else
        printPlayer(loadingdata.songdataB, elapsedSeconds, &playlist);    
}

void loadAudioData()
{
    if (nextSong == NULL)
    {
        if (usingSongDataA)
            loadingdata.loadA = false;
        else
            loadingdata.loadA = true;
            
        loadNext(&loadingdata);
    }         
}

void handleInput()
{
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
        toggleRepeat();
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
        skipToNextSong();
        break;
    case EVENT_PREV:
        skipToPrevSong();
        break;
    case EVENT_ADDTOMAINPLAYLIST:
        addToPlaylist();
        break;
    case EVENT_DELETEFROMMAINPLAYLIST:
        //FIXME implement this
        break;
    case EVENT_EXPORTPLAYLIST:
        savePlaylist();
        break;
    default:
        break;
    }    
}

bool isEndOfList()
{
    return (userData.endOfListReached == 1);
}

int play(SongInfo song)
{
    freeAudioBuffer();
    
    pthread_mutex_init(&(loadingdata.mutex), NULL);
    usingSongDataA = true;
    loadingdata.loadA = true;
    loadSong(currentSong, &loadingdata);
    
    while(!loadedSong)
    {
        usleep(1000);
    }
    userData.currentFileIndex = 0;
    userData.currentPCMFrame = 0;
    loadedSong = false;
    userData.currentFileIndex = 0;
    userData.currentPCMFrame = 0;
    userData.pcmFileA.pcmData = loadingdata.songdataA->pcmFile;
    userData.pcmFileA.filename = strdup(song.filePath);
    userData.pcmFileA.pcmDataSize = loadingdata.songdataA->pcmFileSize;                
    createAudioDevice(&userData);

    firstCall = false;
    loadedSong = false;
    nextSong = NULL;
    refresh = true;

    clock_gettime(CLOCK_MONOTONIC, &start_time);
    calculatePlayListDuration(&playlist);

    while (true)
    {        
        calcElapsedTime();
        handleInput();

        if (resizeFlag)
            resize();
        else
            refreshPlayer();

        if (currentSong->next != NULL)
        {
            if (!loadedSong)
            {
                assignedToUserdata = false;
                loadAudioData();           
            }
            else if (!assignedToUserdata)
                assignUserData();
        }
        if (shouldQuit || isEndOfList())
            break;

        if (isPlaybackDone())
        {
            while(!loadedSong && !loadingFailed)
            {
                usleep(10000);
            }
            if (!assignedToUserdata)
                assignUserData();            
            prepareNextSong();   
        }
        if (currentSong == NULL || isPlaybackOfListDone())
            break;

        usleep(100000);
    }
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
    cleanup();
    restoreTerminalMode();
    enableInputBuffering();    
    setConfig();
    saveMainPlaylist(settings.path, playingMainPlaylist);
    freeAudioBuffer();
    free(nextSong);
    free(mainPlaylist); 
    deleteCache(tempCache);
    deleteTempDir();
    showCursor();
    printf("\n");
   
    return 0;
}

void initResize()
{
    signal(SIGWINCH, handleResize);

    struct sigaction sa;
    sa.sa_handler = resetResizeFlag;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, NULL);    
}

void init()
{
    disableInputBuffering();
    freopen("/dev/null", "w", stderr);
    initResize();
    enableScrolling();
    setNonblockingMode();
    srand(time(NULL));
    tempCache = createCache();
    strcpy(loadingdata.filePath, "");
    loadingdata.songdataA = NULL;
    loadingdata.songdataB = NULL;
    loadingdata.loadA = false;
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