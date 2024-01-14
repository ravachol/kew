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
#include "directorytree.h"

extern const char VERSION[];
extern bool coverEnabled;
extern bool uiEnabled;
extern bool coverAnsi;
extern bool visualizerEnabled;
extern bool useThemeColors;
extern bool hasPrintedPaused;
extern bool quitAfterStopping;
extern bool nerdFontsEnabled;
extern int numProgressBars;
extern int elapsed;
extern int chosenSong;
extern bool resetPlaylistDisplay;
extern int visualizerHeight;
extern volatile bool refresh;
extern TagSettings metadata;
extern bool fastForwarding;
extern bool rewinding;
extern double elapsedSeconds;
extern double pauseSeconds;
extern double totalPauseSeconds;
extern double seekAccumulatedSeconds;
extern double duration;
extern bool allowChooseSongs;
extern int chosenLibRow;
extern int chosenRow;
extern int chosenNodeId;

typedef enum {
    SONG_VIEW,
    KEYBINDINGS_VIEW,
    PLAYLIST_VIEW,
    LIBRARY_VIEW
} ViewState;

typedef struct {
    ViewState currentView;
} AppState;

extern AppState appState;

bool hasNerdFonts();

void createLibrary();

int printPlayer(SongData *songdata, double elapsedSeconds, PlayList *playlist);

void flipNextPage();

void flipPrevPage();

void showHelp(void);

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

void showTrack();

void setTextColorRGB2(int r, int g, int b);

void freeMainDirectoryTree();

#endif
