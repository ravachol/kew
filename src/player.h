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
#include "metadata.h"

extern const char VERSION[];

extern bool coverEnabled;
extern bool coverBlocks;
extern bool printInfo;
extern bool equalizerEnabled;
extern bool equalizerBlocks;
extern int equalizerHeight;
extern bool refresh;
extern TagSettings metadata;

int printPlayer(const char *songFilepath, TagSettings *metaData, double elapsedSeconds, double songDurationSeconds, PlayList *playlist);

void showVersion();

void showHelp();

#endif