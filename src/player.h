#ifndef PLAYER_H
#define PLAYER_H
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "albumart.h"
#include "term.h"
#include "printfunc.h"
#include "visuals.h"
#include "playlist.h"
#include "songloader.h"

extern const char VERSION[];

extern bool coverEnabled;
extern bool uiEnabled;
extern bool coverAnsi;
extern bool printInfo;
extern bool gotosong;
extern bool printKeyBindings;
extern bool visualizerEnabled;
extern bool useThemeColors;
extern int visualizerHeight;
extern volatile bool refresh;
extern TagSettings metadata;

int printPlayer(SongData *songdata, double elapsedSeconds, PlayList *playlist);

void showVersion();

int printAbout();

void showHelp();

#endif