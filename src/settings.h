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
    char coverBlocks[2];
    char equalizerEnabled[2];
    char equalizerHeight[6];
    char equalizerBlocks[2];    
} AppSettings;

extern AppSettings settings;

extern const char PATH_SETTING_FILENAME[];
extern const char SETTINGS_FILENAME[];

extern AppSettings constructAppSettings(KeyValuePair *pairs, int count);

KeyValuePair *readKeyValuePairs(const char *file_path, int *count);

void freeKeyValuePairs(KeyValuePair *pairs, int count);

AppSettings constructAppSettings(KeyValuePair *pairs, int count);

int saveSettingsDeprecated(char *path, const char *settingsFile);

int getSettingsDeprecated(char *path, int len, const char *settingsFile);

void getConfig();

void setConfig();

int getMusicLibraryPath(char *path);

#endif