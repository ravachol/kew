#ifndef PLAYER_H
#define PLAYER_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/imgtotxt/write_ascii.h"
#include "../include/imgtotxt/options.h"
#include "chafafunc.h"
#include "directorytree.h"
#include "playlist.h"
#include "playlist_ui.h"
#include "search_ui.h"
#include "songloader.h"
#include "sound.h"
#include "term.h"
#include "utils.h"
#include "visuals.h"
#include "common_ui.h"

extern bool coverEnabled;
extern bool uiEnabled;
extern bool coverAnsi;
extern bool visualizerEnabled;
extern bool useThemeColors;
extern bool hasPrintedPaused;
extern bool quitAfterStopping;
extern bool nerdFontsEnabled;
extern bool hideLogo;
extern bool hideHelp;
extern int numProgressBars;
extern int chosenSong;
extern bool resetPlaylistDisplay;
extern int visualizerHeight;
extern TagSettings metadata;
extern bool fastForwarding;
extern bool rewinding;
extern double elapsedSeconds;
extern double pauseSeconds;
extern double totalPauseSeconds;
extern double seekAccumulatedSeconds;
extern bool allowChooseSongs;
extern int chosenLibRow;
extern int chosenSearchResultRow;
extern int chosenRow;
extern int chosenNodeId;
extern int cacheLibrary;
extern int numDirectoryTreeEntries;

extern FileSystemEntry *library;

bool hasNerdFonts();

int printPlayer(SongData *songdata, double elapsedSeconds, AppSettings *settings);

void flipNextPage();

void flipPrevPage();

void showHelp(void);

void setChosenDir(FileSystemEntry *entry);

int printAbout(SongData *songdata);

FileSystemEntry *getCurrentLibEntry();

FileSystemEntry *getChosenDir();

FileSystemEntry *getLibrary();

void scrollNext(void);

void scrollPrev(void);

void setCurrentAsChosenDir();

void toggleShowKeyBindings(void);

void toggleShowLibrary();

void toggleShowPlaylist(void);

void toggleShowSearch(void);

void showTrack();

void setTextColorRGB2(int r, int g, int b);

void freeMainDirectoryTree();

char *getLibraryFilePath();

void resetChosenDir();

void tabNext();

#endif
