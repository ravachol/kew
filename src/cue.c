// cue - command-line music player
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
#include "visuals.h"

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

#ifndef MAX_EVENTS_IN_QUEUE
#define MAX_EVENTS_IN_QUEUE 1
#endif
#define TRESHOLD_TERMINAL_SIZE 28

int barHeightMax = 4;

const char VERSION[] = "0.9.5";
const char SETTINGS_FILENAME[] = ".cue-settings";
const char ALLOWED_EXTENSIONS[] = "\\.(m4a|mp3|ogg|flac|wav|aac|wma|raw|mp4a|mp4|m3u|pls)$";
char durationFilePath[FILENAME_MAX];
char tagsFilePath[FILENAME_MAX];
bool isResizing = false;
bool escapePressed = false;
bool visualizerEnabled = false;
int progressLine = 1;
struct timespec escapeTime;
PlayList playlist = {NULL, NULL};
Node *currentSong;

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
    save_cursor_position();
  }
  else
  {
    restore_cursor_position();
  }

  char input = readInput();

  if (input == 'v')
  {
    visualizerEnabled = !visualizerEnabled;
    clear_screen();
  }

  if (input == 27)
  { // ASCII value of escape key
    escapePressed = true;
    clock_gettime(CLOCK_MONOTONIC, &escapeTime);
    restore_cursor_position();
    return event;
  }
  else
  {
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
  clear_screen();
  escapePressed = false;
  cleanupPlaybackDevice();
  deleteFile(tagsFilePath);
  deleteFile(durationFilePath);
}

void handleResize(int signal)
{
  isResizing = true;
}

int play(const char *filepath)
{
  char musicFilepath[MAX_FILENAME_LENGTH];
  struct timespec start_time;
  double elapsed_seconds = 0.0;
  bool shouldQuit = false;
  bool skip = false;
  int col = 1;
  generateTempFilePath(durationFilePath, "duration", ".txt");
  double songLength = getDuration(filepath, durationFilePath);
  strcpy(musicFilepath, filepath);
  int term_w, term_h;
  int asciiHeight, asciiWidth;
  get_term_size(&term_w, &term_h);
  if (term_h <= TRESHOLD_TERMINAL_SIZE)  
  {
    asciiHeight = small_ascii_height;
    asciiWidth = small_ascii_width;
  } 
  else {
    asciiHeight = default_ascii_height;
    asciiWidth = default_ascii_width;

  }
  int foundArt = displayAlbumArt(getDirectoryFromPath(filepath), asciiHeight, asciiWidth);
  if (foundArt < 0)
    visualizerEnabled = true;
  generateTempFilePath(tagsFilePath, "metatags", ".txt");
  extract_tags(strdup(filepath), tagsFilePath);
  clock_gettime(CLOCK_MONOTONIC, &start_time);
  int res = playSoundFile(musicFilepath);
  setDefaultTextColor();
  printBasicMetadata(tagsFilePath);
  get_cursor_position(&progressLine, &col);
  fflush(stdout);
  usleep(100000);
  float dynamicRange = 1.0f;
  escapePressed = false;
  shouldQuit = false;
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
    case EVENT_QUIT:
      shouldQuit = true;
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
        if (visualizerEnabled) 
        {
          drawSpectrum(barHeightMax, asciiWidth);
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
  getsettings(path, MAXPATHLEN, SETTINGS_FILENAME);

  if (path[0] == '\0') // if NULL, ie no path setting was found
  {
    struct passwd *pw = getpwuid(getuid());
    strcat(path, pw->pw_dir);
    strcat(path, "/Music/");
  }
  return 0;
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

  return 0;
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