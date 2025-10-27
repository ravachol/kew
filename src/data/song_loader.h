/**
 * @file song_loader.h
 * @brief Song loading and preparation routines.
 *
 * Responsible for loading song data from file
 */

#include "common/appstate.h"

#include <stdbool.h>

SongData *load_song_data(char *filePath);
void unload_song_data(SongData **songdata);
