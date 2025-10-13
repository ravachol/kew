/**
 * @file playlist_ui.[h]
 * @brief Playlist display and interaction layer.
 *
 * Renders the list of tracks in the current playlist and handles
 * navigation, selection, and editing within the playlist view.
 */

#ifndef PLAYLIST_UI_H
#define PLAYLIST_UI_H

#include "common/appstate.h"

int displayPlaylist(PlayList *list, int maxListSize, int indent,
                    int *chosenSong, int *chosenNodeId, bool reset);
void setEndOfListReached(void);

#endif
