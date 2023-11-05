#ifndef SETTINGS_H
#define SETTINGS_H
#include <pwd.h>
#include <unistd.h>
#include <sys/param.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "player.h"
#include "file.h"

#ifndef KEYVALUEPAIR_STRUCT
#define KEYVALUEPAIR_STRUCT

typedef struct
{
        char *key;
        char *value;
} KeyValuePair;

#endif

typedef struct
{
    char path[MAXPATHLEN];
    char coverEnabled[2];
    char coverAnsi[2];
    char useProfileColors[2];
    char visualizerEnabled[2];
    char visualizerHeight[6];
    char togglePlaylist[20];
    char toggleBindings[20];
    char volumeUp[20];
    char volumeDown[20];
    char previousTrackAlt[20];
    char nextTrackAlt[20];
    char scrollUpAlt[20];
    char scrollDownAlt[20];
    char switchNumberedSong[20];
    char switchNumberedSongAlt[20];
    char switchNumberedSongAlt2[20];
    char togglePause[20];
    char toggleColorsDerivedFrom[20];
    char toggleVisualizer[20];
    char toggleCovers[20];
    char toggleAscii[20];
    char toggleRepeat[20];
    char toggleShuffle[20];
    char seekBackward[20];
    char seekForward[20];
    char savePlaylist[20];
    char quit[20];
} AppSettings;

extern AppSettings settings;

extern const char PATH_SETTING_FILENAME[];
extern const char SETTINGS_FILENAME[];

extern AppSettings constructAppSettings(KeyValuePair *pairs, int count);

KeyValuePair *readKeyValuePairs(const char *file_path, int *count);

void freeKeyValuePairs(KeyValuePair *pairs, int count);

AppSettings constructAppSettings(KeyValuePair *pairs, int count);

void getConfig();

void setConfig();

int getMusicLibraryPath(char *path);

#endif