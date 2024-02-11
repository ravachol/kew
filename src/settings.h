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

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

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
        char togglePlaylist[6];
        char toggleBindings[6];
        char volumeUp[6];
        char volumeUpAlt[6];        
        char volumeDown[6];
        char previousTrackAlt[6];
        char nextTrackAlt[6];
        char scrollUpAlt[6];
        char scrollDownAlt[6];
        char switchNumberedSong[6];
        char switchNumberedSongAlt[6];
        char switchNumberedSongAlt2[6];
        char togglePause[6];
        char toggleColorsDerivedFrom[6];
        char toggleVisualizer[6];
        char toggleCovers[6];
        char toggleAscii[6];
        char toggleRepeat[6];
        char toggleShuffle[6];
        char seekBackward[6];
        char seekForward[6];
        char savePlaylist[6];
        char addToMainPlaylist[6];
        char quit[6];
        char hardSwitchNumberedSong[6];
        char hardPlayPause[6];
        char hardPrev[6];
        char hardNext[6];
        char hardScrollUp[6];
        char hardScrollDown[6];
        char hardShowInfo[6];
        char hardShowInfoAlt[6];
        char hardShowKeys[6];
        char hardShowKeysAlt[6];
        char hardEndOfPlaylist[6];
        char hardShowLibrary[6];        
        char hardShowLibraryAlt[6];
        char hardShowTrack[6];
        char hardShowTrackAlt[6];
        char hardNextPage[6];
        char hardPrevPage[6];
        char hardRemove[6];
        char lastVolume[6];
        char allowNotifications[2];
} AppSettings;

extern AppSettings settings;

extern const char PATH_SETTING_FILENAME[];
extern const char SETTINGS_FILENAME[];

extern AppSettings constructAppSettings(KeyValuePair *pairs, int count);

KeyValuePair *readKeyValuePairs(const char *file_path, int *count);

void freeKeyValuePairs(KeyValuePair *pairs, int count);

AppSettings constructAppSettings(KeyValuePair *pairs, int count);

void getConfig(void);

void setConfig(void);

int getMusicLibraryPath(char *path);

#endif
