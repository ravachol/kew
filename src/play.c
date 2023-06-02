// play - command-line music player
// Copyright (C) 2022 Ravachol
//
// http://github.com/ravachol/play
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
#include "sound.h"
#include "stringextensions.h"
#include "dir.h"
#include "albumart.h"
#include "settings.h"
#include "printfunc.h"
#include "volume.h"
#include "playlist.h"
#include "events.h"
#include "file.h"

#define CLOCK_MONOTONIC 1
#define MAX_VOL_UP_EVENTS 3
#define MAX_EVENTS_IN_QUEUE 1

const char VERSION[] = "0.9.4";
const char SETTINGS_FILENAME[] = ".play-settings";
const char ALLOWED_EXTENSIONS[] = "\\.(m4a|mp3|ogg|flac|wav|aac|wma|raw|mp4a|mp4|m3u|pls)$";
char durationFilePath[FILENAME_MAX];
char tagsFilePath[FILENAME_MAX]; 
bool isResizing = false;
bool escapePressed = false;
int originalLine = -1;
struct timespec escapeTime;
PlayList playlist = {NULL, NULL};
Node *currentSong;

void handleResize(int signal)
{
  struct winsize ws;
  ioctl(0, TIOCGWINSZ, &ws);
  int rows = ws.ws_row;

  printf("\033[1;%dr", rows); // Set scrolling region from row 1 to the last row
  printf("\033[%d;1H", rows); // Move cursor to the last row
  fflush(stdout);             // Flush the output buffer
  moveCursorToLastLine();
  isResizing = true;
}

struct Event processInput()
{
  struct Event event;
  event.type = EVENT_NONE;
  event.key = '\0';

  if (escapePressed)
  {
    struct timespec currentTime;
    clock_gettime(CLOCK_MONOTONIC, &currentTime);
    double elapsedSeconds = difftime(currentTime.tv_sec, escapeTime.tv_sec);

    if (elapsedSeconds > 1.0)
    {
      event.type = EVENT_QUIT;
      escapePressed = false;
      return event;
    }
  }

  if (!isInputAvailable())
  {
    if (!isEventQueueEmpty())
      event = dequeueEvent();
    return event;
  }

  char input = readInput();

  if (input == 27)
  { // ASCII value of escape key
    escapePressed = true;
    clock_gettime(CLOCK_MONOTONIC, &escapeTime);
    save_cursor_position();
  }
  else
  {
    escapePressed = false;
    event.type = EVENT_KEY_PRESS;
    event.key = input;

    switch (event.key)
    {
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
    case ' ': // Space
      event.type = EVENT_PLAY_PAUSE;
      break;    
    default:
      break;
    }
    enqueueEvent(&event);
  }
  return event;
}

void cleanup() {
  cleanupPlaybackDevice();
  deleteFile(tagsFilePath);
  deleteFile(durationFilePath);
}

double getSongLength(const char* songPath)
{
  generateTempFilePath(durationFilePath, "duration", ".txt");
  return getDuration(songPath, durationFilePath);
}

int play(const char *filepath)
{
  char musicFilepath[MAX_FILENAME_LENGTH];
  int min_cursor_line = 0;
  struct timespec start_time;
  double elapsed_seconds = 0.0;  
  bool shouldQuit = false;
  bool skip = false;
  int row = 1;
  int col = 1;   

  generateTempFilePath(durationFilePath, "duration", ".txt");  
  double songLength = getDuration(filepath, durationFilePath); 
  get_cursor_position(&row, &col);
  originalLine = row;    
  strcpy(musicFilepath, filepath);
  displayAlbumArt(getDirectoryFromPath(filepath));    
  generateTempFilePath(tagsFilePath, "metatags", ".txt");    
  extract_tags(strdup(filepath), tagsFilePath);
  clock_gettime(CLOCK_MONOTONIC, &start_time);  
  int res = playSoundFile(musicFilepath);  
  setTextColorRGB(200,200,200); // white text  
  printBasicMetadata(tagsFilePath);

  if (res != 0) {
    printf("\033[%dB", originalLine);
    cleanup();
    currentSong = getListNext(&playlist, currentSong);
    if (currentSong != NULL)
      return play(currentSong->song.filePath);
  }
  while (true)
  {
    moveCursorToLastLine();
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);

    elapsed_seconds = (double)(current_time.tv_sec - start_time.tv_sec) +
                      (double)(current_time.tv_nsec - start_time.tv_nsec) / 1e9;

    if (isResizing) {
      isResizing = false;
    }
    else if (elapsed_seconds < songLength) {
      moveCursorToLastLine();
      printProgress(elapsed_seconds, songLength, 999);
    }

    // Process events
    struct Event event = processInput();

    switch (event.type) {
      case EVENT_PLAY_PAUSE:
        pausePlayback();
        break;      
      case EVENT_QUIT:
        shouldQuit = true;
        break;
      case EVENT_VOLUME_UP:
        adjustVolumePercent(2);
        break;
      case EVENT_VOLUME_DOWN:
        adjustVolumePercent(-2);
        break;
      case EVENT_NEXT:
        printf("\033[%dB", originalLine);
      cleanup();
        currentSong = getListNext(&playlist, currentSong);
        if (currentSong != NULL)
          return play(currentSong->song.filePath);
        else
          return -1;
        break;
      case EVENT_PREV:
        printf("\033[%dB", originalLine);
        cleanup();
        currentSong = getListPrev(&playlist, currentSong);
        if (currentSong != NULL)
          return play(currentSong->song.filePath); 
        else
          return -1;
        break;
      default:
        break;
    }

    if (shouldQuit) {
      moveCursorToLastLine();
      break;
    }

    if (isPlaybackDone()) {
      printf("\033[%dB", originalLine); 
      currentSong = getListNext(&playlist, currentSong);
      if (currentSong != NULL)
        play(currentSong->song.filePath); 
      break;      
    }

    // Add a small delay to avoid excessive updates
    usleep(100000);
  }
  cleanup();
  return 0;
}

int getMusicLibraryPath(char* path)
{
    getsettings(path, MAXPATHLEN, SETTINGS_FILENAME);

    if (path[0] == '\0') // if NULL, ie no path setting was found
    {
      struct passwd *pw = getpwuid(getuid());
      strcat(path, pw->pw_dir);
      strcat(path, "/Music/");
    }  
}

int start()
{
  if (&playlist == NULL || playlist.head == NULL)
    return -1;  
  if (currentSong == NULL)
    currentSong = playlist.head;
  SongInfo song  = currentSong->song;
  char *path = song.filePath;
  play(path);
  restoreTerminalMode();
  return 0;
}

void init()
{
  freopen("/dev/null", "w", stderr);  
  signal(SIGWINCH, handleResize);
  initEventQueue();
  enableScrolling();
  setNonblockingMode();     
}

int makePlaylist(int argc, char *argv[]) 
{
    enum SearchType searchType = Any;  

    if (strcmp(argv[1], "dir") == 0)
      searchType = DirOnly;
    else if (strcmp(argv[1], "song") == 0)
      searchType = FileOnly;

    int start = 2;

    if (searchType == FileOnly || searchType == DirOnly)
      start = 3;    

    // create search string
    int size = 256;
    char search[size];
    strncpy(search, argv[start - 1], size - 1);
    for (int i = start; i < argc; i++)
    {
      strcat(search, " ");
      strcat(search, argv[i]);
    }

    char buf[MAXPATHLEN] = {0};
    char path[MAXPATHLEN] = {0};
    getMusicLibraryPath(path);
    if (walker(path, search, buf, ALLOWED_EXTENSIONS, searchType) == 0)
    {      
      buildPlaylistRecursive(buf, ALLOWED_EXTENSIONS, &playlist);
    }
    else
    {
      puts("Music not found");
    }
}

int main(int argc, char *argv[])
{  
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
    saveSettings(argv[2], SETTINGS_FILENAME);
  }
  else if (argc >= 2)
  {    
    init();
    makePlaylist(argc, argv);    
    start();
  }
  return 0;
}