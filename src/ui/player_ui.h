/**
 * @file player_ui.h
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

int printLogoArt(const UISettings *ui, int indent, bool centered, bool printTagLine, bool useGradient);
int calcIndentNormal(void);
int printPlayer(SongData *songdata, double elapsedSeconds);
int getFooterRow(void);
int getFooterCol(void);
int getIndent(void);
int printAbout(SongData *songdata);
int getChosenRow(void);
void flipNextPage(void);
void flipPrevPage(void);
void showHelp(void);
void setChosenDir(FileSystemEntry *entry);
void setCurrentAsChosenDir(void);
void scrollNext(void);
void scrollPrev(void);
void toggleShowView(ViewState VIEW_TO_SHOW);
void showTrack(void);
void freeMainDirectoryTree(void);
void resetChosenDir(void);
void switchToNextView(void);
void switchToPreviousView(void);
void resetSearchResult(void);
void setChosenRow(int row);
void refreshPlayer();
void setTrackTitleAsWindowTitle(void);
char *getLibraryFilePath(void);
bool initTheme(int argc, char *argv[]);
FileSystemEntry *getChosenDir(void);

#endif
