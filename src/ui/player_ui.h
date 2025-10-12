/**
 * @file player_ui.[h]
 * @brief Main player screen rendering.
 *
 * Displays current track info, progress bar, and playback status.
 * Acts as the central visual component of the terminal player.
 */

#ifndef PLAYER_H
#define PLAYER_H

#include "common/appstate.h"

#include "data/directorytree.h"

#include <stdbool.h>

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

int printLogoArt(const UISettings *ui, int indent, bool centered, bool printTagLine, bool useGradient);

int calcIndentNormal(void);

int printPlayer(SongData *songdata, double elapsedSeconds, AppSettings *settings, AppState *appState);

int getFooterRow(void);

int getFooterCol(void);

void flipNextPage(AppState *state);

void flipPrevPage(AppState *state);

void showHelp(void);

void setChosenDir(FileSystemEntry *entry);

int getIndent(void);

int printAbout(SongData *songdata, UISettings *ui);

FileSystemEntry *getChosenDir(void);

void setCurrentAsChosenDir(void);

void scrollNext(AppState *state);

void scrollPrev(AppState *state);

void toggleShowView(ViewState VIEW_TO_SHOW, AppState *state);

void showTrack(AppState *state);

void freeMainDirectoryTree(AppState *state);

char *getLibraryFilePath(void);

void resetChosenDir(void);

void switchToNextView(AppState *state);

void switchToPreviousView(AppState *state);

void resetSearchResult(void);

int getChosenRow(void);

void setChosenRow(int row);

#endif
