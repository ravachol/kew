#ifndef PLAYLIST_UI_H
#define PLAYLIST_UI_H

#include "appstate.h"
#include "playlist.h"

int displayPlaylist(PlayList *list, int maxListSize, int indent,
                    int *chosenSong, int *chosenNodeId, bool reset,
                    AppState *state);

#endif
