#ifndef PLAYER_H
#define PLAYER_H
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "albumart.h"
#include "term.h"
#include "printfunc.h"
#include "visuals.h"
#include "playlist.h"

extern const char VERSION[];

extern bool coverEnabled;
extern bool coverBlocks;
extern bool visualizationEnabled;
extern int visualizationHeight;
extern bool refresh;

int printPlayer(const char *songFilepath, const char *tagsFilePath, double elapsedSeconds, double songDurationSeconds, PlayList *playlist);

int fetchLatestVersion(int* major, int* minor, int* patch);

void showVersion();

void showHelp();

#endif