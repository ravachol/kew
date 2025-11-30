/**
 * @file playlist_ui.h
 * @brief Playlist display and interaction layer.
 *
 * Renders the list of tracks in the current playlist and handles
 * navigation, selection, and editing within the playlist view.
 */

#ifndef PLAYLIST_UI_H
#define PLAYLIST_UI_H

#include "common/appstate.h"

int display_playlist(int row, int col, PlayList *list, int max_list_size,
                     int *chosen_song, int *chosen_node_id, bool reset);
void set_end_of_list_reached(void);

#endif
