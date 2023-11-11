#ifndef PLAYER_H
#define PLAYER_H
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "albumart.h"
#include "cutils.h"
#include "term.h"
#include "visuals.h"
#include "playlist.h"
#include "songloader.h"
#include "soundgapless.h"

extern const char VERSION[];
extern bool coverEnabled;
extern bool uiEnabled;
extern bool coverAnsi;
extern bool printInfo;
extern bool printKeyBindings;
extern bool visualizerEnabled;
extern bool useThemeColors;
extern int numProgressBars;
extern int elapsed;
extern double duration;
extern int chosenSong;
extern bool resetPlaylistDisplay;
extern int visualizerHeight;
extern volatile bool refresh;
extern TagSettings metadata;

int printPlayer(SongData *songdata, double elapsedSeconds, PlayList *playlist);

void showVersion();

int printAbout();

void showHelp();

void scrollNext();

void scrollPrev();

void toggleShowKeyBindings();

void toggleShowPlaylist();

void setTextColorRGB2(int r, int g, int b);

#endif