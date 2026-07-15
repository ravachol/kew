/**
 * @file playlist_ui.h
 * @brief Playlist display and interaction layer.
 *
 * Renders the list of tracks in the current playlist and handles
 * navigation, selection, and editing within the playlist view.
 */

#ifndef PLAYLIST_UI_H
#define PLAYLIST_UI_H

#include "data/playlist_type.h"

#include <stdbool.h>

/**
 * @brief Returns the row number that the playlist should start showing.
 *
 * Should be called after determine_start_node.
 */
int determine_playlist_start(int *previous_start_iter, int found_at, int max_list_size,
                             int *chosen_song, bool reset, bool end_of_list_reached, bool center);

/**
 * @brief Returns the node that the playlist should start showing.
 *
 */
Node *determine_start_node(Node *head, int *found_at, int list_size);


/**
 * @brief Sets the state indicating that the end of the playlist has been reached.
 *
 * This function updates various states related to playback and song data when the
 * end of the playlist is reached. It stops the current playback, resets necessary states,
 * and prepares for either repeating the playlist or switching views.
 */
void set_end_of_list_reached(void);

#endif
