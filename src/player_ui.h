#ifndef PLAYER_H
#define PLAYER_H

#include <stdbool.h>
#include "appstate.h"
#include "common.h"
#include "directorytree.h"
#include "soundcommon.h"


#ifndef TAGSETTINGS_STRUCT
#define TAGSETTINGS_STRUCT

#define METADATA_MAX_LENGTH 256

typedef struct
{
        char title[METADATA_MAX_LENGTH];
        char artist[METADATA_MAX_LENGTH];
        char album_artist[METADATA_MAX_LENGTH];
        char album[METADATA_MAX_LENGTH];
        char date[METADATA_MAX_LENGTH];
        double replaygainTrack;
        double replaygainAlbum;
} TagSettings;

#endif

#ifndef SONGDATA_STRUCT
#define SONGDATA_STRUCT
typedef struct
{
        gchar *trackId;
        char filePath[MAXPATHLEN];
        char coverArtPath[MAXPATHLEN];
        unsigned char red;
        unsigned char green;
        unsigned char blue;
        TagSettings *metadata;
        unsigned char *cover;
        int avgBitRate;
        int coverWidth;
        int coverHeight;
        double duration;
        bool hasErrors;
} SongData;
#endif


extern int numProgressBars;

extern bool fastForwarding;
extern bool rewinding;

extern FileSystemEntry *library;

int printPlayer(SongData *songdata, double elapsedSeconds, AppSettings *settings, AppState *appState);

void flipNextPage(void);

void flipPrevPage(void);

void showHelp(void);

void setChosenDir(FileSystemEntry *entry);

int printAbout(SongData *songdata, UISettings *ui);

FileSystemEntry *getCurrentLibEntry(void);

FileSystemEntry *getChosenDir(void);

FileSystemEntry *getLibrary(void);

void scrollNext(void);

void scrollPrev(void);

void setCurrentAsChosenDir(void);

void toggleShowView(ViewState VIEW_TO_SHOW);

void toggleShowRadioSearch(void);

void showTrack(void);

void freeMainDirectoryTree(AppState *state);

char *getLibraryFilePath(void);

void resetChosenDir(void);

void switchToNextView(void);

void switchToPreviousView(void);

void resetSearchResult(void);

void resetRadioSearchResult(void);

int getChosenRow(void);

void setChosenRow(int row);

#endif
