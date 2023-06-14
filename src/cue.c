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
#include "sound.h"
#include "stringextensions.h"
#include "dir.h"
#include "settings.h"
#include "printfunc.h"
#include "playlist.h"
#include "events.h"
#include "file.h"
#include "visuals.h"
#include "albumart.h"

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

#ifndef MAX_EVENTS_IN_QUEUE
#define MAX_EVENTS_IN_QUEUE 1
#endif
#define TRESHOLD_TERMINAL_SIZE 28

const char VERSION[] = "1.0.0";
const char PATH_SETTING_FILENAME[] = ".cue-settings";
const char SETTINGS_FILENAME[] = "cue.conf";
const char ALLOWED_EXTENSIONS[] = "\\.(m4a|mp3|ogg|flac|wav|aac|wma|raw|mp4a|mp4|m3u|pls)$";
char tagsFilePath[FILENAME_MAX];
bool isResizing = false;
bool escapePressed = false;
bool visualizationEnabled = false;
bool coverEnabled = true;
bool coverBlocks = false;
bool repeatEnabled = false;
int visualizationHeight = 4;
int progressLine = 1;
struct timespec escapeTime;
PlayList playlist = {NULL, NULL};
Node *currentSong;
AppSettings settings;

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
    saveCursorPosition();
  }
  else
  {
    restoreCursorPosition();
  }

  char input = readInput();

  if (input == 27)
  { // ASCII value of escape key
    escapePressed = true;
    clock_gettime(CLOCK_MONOTONIC, &escapeTime);
    restoreCursorPosition();
    return event;
  }
  else
  {
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
    case ' ': // Space
      event.type = EVENT_PLAY_PAUSE;
      break;
    default:
      break;
    }
    enqueueEvent(&event);
  }

  if (event.key == 'A' || event.key == 'B' || event.key == 'C' || event.key == 'D' || event.key == ' ')
  {
    clock_gettime(CLOCK_MONOTONIC, &escapeTime);
    escapePressed = false;
  }

  if (!isEventQueueEmpty())
    event = dequeueEvent();

  return event;
}

void cleanup()
{
  clearRestOfScreen();
  escapePressed = false;
  cleanupPlaybackDevice();
  deleteFile(tagsFilePath);
}

void handleResize(int signal)
{
  isResizing = true;
}

int play(const char *filepath)
{
  escapePressed = false;
  char musicFilepath[MAX_FILENAME_LENGTH];
  struct timespec start_time;
  double elapsed_seconds = 0.0;
  bool shouldQuit = false;
  bool skip = false;
  int col = 1;
  double songLength = getDuration(filepath);
  strcpy(musicFilepath, filepath);
  int term_w, term_h;
  int asciiHeight, asciiWidth;
  getTermSize(&term_w, &term_h);
  if (term_h <= TRESHOLD_TERMINAL_SIZE)  
  {
    asciiHeight = small_ascii_height;
    asciiWidth = small_ascii_width;
  } 
  else {
    asciiHeight = default_ascii_height;
    asciiWidth = default_ascii_width;

  }
  if (coverEnabled)
  {
    int foundArt = displayAlbumArt(filepath, asciiHeight, asciiWidth, coverBlocks);
    if (foundArt < 0)
      visualizationEnabled = true;
  }
  generateTempFilePath(tagsFilePath, "metatags", ".txt");
  extract_tags(strdup(filepath), tagsFilePath);
  clock_gettime(CLOCK_MONOTONIC, &start_time);
  int res = playSoundFile(musicFilepath);
  setDefaultTextColor();
  printBasicMetadata(tagsFilePath);
  getCursorPosition(&progressLine, &col);
  fflush(stdout);
  usleep(100000);
  clock_gettime(CLOCK_MONOTONIC, &escapeTime);

  if (res != 0)
  {
    cleanup();
    currentSong = getListNext(&playlist, currentSong);
    if (currentSong != NULL)
      return play(currentSong->song.filePath);
  }
  while (true)
  {
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);

    elapsed_seconds = (double)(current_time.tv_sec - start_time.tv_sec) +
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
      break;
    case EVENT_TOGGLEBLOCKS:
      coverBlocks = !coverBlocks;
      strcpy(settings.coverBlocks, coverBlocks ? "1" : "0");
      break;      
    case EVENT_SHUFFLE:
      shufflePlaylist(&playlist);  
      break;
    case EVENT_QUIT:
      if (event.key == 'q' || elapsed_seconds > 2.0) {
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
      cleanup();
      currentSong = getListNext(&playlist, currentSong);
      if (currentSong != NULL)
        return play(currentSong->song.filePath);
      else
        return -1;
      break;
    case EVENT_PREV:
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

    if (isResizing)
    {
      usleep(100000);
      isResizing = false;
    }
    else {
      if ((elapsed_seconds < songLength) && !isPaused() )
      {                 
        printProgress(elapsed_seconds, songLength);
        if (visualizationEnabled) 
        {
          drawSpectrum(visualizationHeight, asciiWidth);
        }
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

int getMusicLibraryPath(char *path)
{
  char expandedPath[MAXPATHLEN];

  if (path[0] != '\0' && path[0] != '\r')
  {
    if (expandPath(path, expandedPath) >= 0)
      strcpy(path, expandedPath);
    return 0;
  }
  getSettingsDeprecated(path, MAXPATHLEN, PATH_SETTING_FILENAME);

  if (path[0] == '\0') // if NULL, ie no path setting was found
  {
    struct passwd *pw = getpwuid(getuid());
    strcat(path, pw->pw_dir);
    strcat(path, "/Music/");
  }

  if (expandPath(path, expandedPath))
    strcpy(path, expandedPath);

  return 0;
}


void getConfig(const char *filename)
{
  int pair_count;
  struct passwd *pw = getpwuid(getuid());
  const char *homedir = pw->pw_dir;
  const char *filepath = strcat(strcat(strcpy((char*)malloc(strlen(homedir) + strlen("/") + strlen(filename) + 1), homedir), "/"),filename);
  KeyValuePair *pairs = readKeyValuePairs(filepath, &pair_count);
  settings = constructAppSettings(pairs, pair_count);

  coverEnabled = (settings.coverEnabled[0] == '1');
  coverBlocks = (settings.coverBlocks[0] == '1');
  visualizationEnabled = (settings.visualizationEnabled[0] == '1');
  int temp = atoi(settings.visualizationHeight);
  if (temp > 0)
    visualizationHeight = temp;
  getMusicLibraryPath(settings.path);
} 

void setConfig(AppSettings *settings, const char *filename)
{
    int pair_count = 4;  // Number of key-value pairs in AppSettings

    // Create the file path
    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;

    char *filepath = (char *)malloc(strlen(homedir) + strlen("/") + strlen(filename) + 1);
    strcpy(filepath, homedir);
    strcat(filepath, "/");
    strcat(filepath, filename);

    // Open the file for writing
    FILE *file = fopen(filepath, "w");
    if (file == NULL) {
        fprintf(stderr, "Error opening file: %s\n", filepath);
        free(filepath);
        return;
    }

    if (settings->coverEnabled[0] == '\0')
      strcpy(settings->coverEnabled, "1");
    if (settings->coverBlocks[0] == '\0')
      strcpy(settings->coverBlocks, "0");
    if (settings->visualizationEnabled[0] == '\0')
      strcpy(settings->visualizationEnabled, "0");
    if (settings->visualizationHeight[0] == '\0') {
      sprintf(settings->visualizationHeight, "%d", visualizationHeight);
    }

    // Null-terminate the character arrays
    settings->path[MAXPATHLEN-1] = '\0';
    settings->coverEnabled[1] = '\0';
    settings->coverBlocks[1] = '\0';
    settings->visualizationEnabled[1] = '\0';
    settings->visualizationHeight[5] = '\0';    

    // Write the settings to the file
    fprintf(file, "path=%s\n", settings->path);
    fprintf(file, "coverEnabled=%s\n", settings->coverEnabled);
    fprintf(file, "coverBlocks=%s\n", settings->coverBlocks);
    fprintf(file, "visualizationEnabled=%s\n", settings->visualizationEnabled);
    fprintf(file, "visualizationHeight=%s\n", settings->visualizationHeight);

    // Close the file and free the allocated memory
    fclose(file);
    free(filepath);
}

int start()
{
  if (playlist.head == NULL)
    return -1;
  if (currentSong == NULL)
    currentSong = playlist.head;
  SongInfo song = currentSong->song;
  char *path = song.filePath;
  play(path);
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

void removeElement(char* argv[], int index, int* argc) {
    if (index < 0 || index >= *argc) {
        // Invalid index
        return;
    }
    
    // Shift elements after the index
    for (int i = index; i < *argc - 1; i++) {
        argv[i] = argv[i + 1];
    }
    
    // Update the argument count
    (*argc)--;
}

int makePlaylist(int argc, char *argv[])
{
  enum SearchType searchType = SearchAny;
  int searchTypeIndex = 1;
  bool shuffle = false;
  const char* delimiter = ":";
  PlayList partialPlaylist = {NULL, NULL};

  if (strcmp(argv[1], "random") == 0 || strcmp(argv[1], "rand") == 0 || strcmp(argv[1], "shuffle") == 0)
  {
    int count = 0;
    while (argv[count] != NULL) {
        count++;
    }
    if (count > 2)
    {
      searchTypeIndex = 2;
      shuffle = true;
    }
  }

  if (strcmp(argv[searchTypeIndex], "dir") == 0)
    searchType = DirOnly;
  else if (strcmp(argv[searchTypeIndex], "song") == 0)
    searchType = FileOnly;

  int start = searchTypeIndex + 1;

  if (searchType == FileOnly || searchType == DirOnly)
    start = searchTypeIndex + 2;

  // create search string
  int size = 256;
  char search[size];
  strncpy(search, argv[start - 1], size - 1);
  for (int i = start; i < argc; i++)
  {
    strcat(search, " ");
    strcat(search, argv[i]);
  }

  if (strstr(search, delimiter))
  {
    if (searchType == SearchAny)
      searchType = DirOnly;
    shuffle = true;
  }

  char* token = strtok(search, delimiter);

  // Subsequent calls to strtok with NULL to continue splitting
  while (token != NULL) {      
      char buf[MAXPATHLEN] = {0};
      char path[MAXPATHLEN] = {0};
      if (strncmp(token, "song", 4) == 0) {
        // Remove "dir" from token by shifting characters
        memmove(token, token + 4, strlen(token + 4) + 1);
        searchType = FileOnly;
      }
      trim(token);
      if (walker(settings.path, token, buf, ALLOWED_EXTENSIONS, searchType) == 0)
      {
        buildPlaylistRecursive(buf, ALLOWED_EXTENSIONS, &partialPlaylist);
        joinPlaylist(&playlist, &partialPlaylist);
      }
      
      token = strtok(NULL, delimiter);
      searchType = DirOnly;      
  }  

  if (shuffle)
      shufflePlaylist(&playlist);  

  if (playlist.head == NULL)
    puts("Music not found");

  return 0;
}

void handleSwitches(int *argc, char *argv[])
{
  const char* nocoverSwitch = "--nocover";
  int idx = -1;
  for (int i = 0; i < *argc; i++) {
    if (strcasestr(argv[i], nocoverSwitch))
    {
      coverEnabled = false;
      idx = i;
    }
  }
  if (idx >= 0)
    removeElement(argv, idx, argc);
}

int main(int argc, char *argv[])
{
  getConfig(SETTINGS_FILENAME);

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
    saveSettingsDeprecated(argv[2], PATH_SETTING_FILENAME);
    strcpy(settings.path, argv[2]);
    setConfig(&settings, SETTINGS_FILENAME);
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