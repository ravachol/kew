#ifndef PLAYLIST_UI_H
#define PLAYLIST_UI_H

#include "common_ui.h"
#include "playlist.h"
#include "songloader.h"
#include "term.h"
#include "utils.h"

int displayPlaylist(PlayList *list, int maxListSize, int indent, int *chosenSong, int *chosenNodeId, bool reset, AppState *state);

#endif
