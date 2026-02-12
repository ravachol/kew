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

/**
 * @brief Displays the playlist items in a terminal-based user interface.
 *
 * This function prints the playlist items starting from the given node and iterates
 * through them up to the maximum number of items specified. The playlist rows are
 * displayed with optional color coding and formatting.
 *
 * @param row The starting row in the terminal where the playlist will be displayed.
 * @param col The starting column in the terminal where the playlist will be displayed.
 * @param list The playlist to display, containing the list of songs/nodes.
 * @param max_list_size The maximum number of playlist items to display.
 * @param chosen_song A pointer to the index of the currently selected song.
 * @param chosen_node_id A pointer to store the node ID of the selected song.
 * @param reset A flag indicating whether the playlist should be reset before displaying.
 *
 * @return The number of rows printed on the terminal.
 */
int display_playlist(int row, int col, PlayList *list, int max_list_size,
                     int *chosen_song, int *chosen_node_id, bool reset);

/**
 * @brief Sets the state indicating that the end of the playlist has been reached.
 *
 * This function updates various states related to playback and song data when the
 * end of the playlist is reached. It stops the current playback, resets necessary states,
 * and prepares for either repeating the playlist or switching views.
 */
void set_end_of_list_reached(void);

#endif
