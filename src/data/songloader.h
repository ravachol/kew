/**
 * @file songloader.h
 * @brief Song loading and preparation routines.
 *
 * Responsible for loading song data from file
 */

#include "common/appstate.h"

#include <stdbool.h>

SongData *loadSongData(char *filePath);
void unloadSongData(SongData **songdata);
