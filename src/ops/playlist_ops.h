/**
 * @file playlist_ops.h
 * @brief Playlist management operations.
 *
 * Implements functionality for adding, removing, reordering,
 * shuffling, and retrieving songs within playlists.
 * Coordinates with the playback system for next/previous transitions.
 */

#ifndef PLAYLIST_OPS_H
#define PLAYLIST_OPS_H

#include "common/appstate.h"

#include "data/playlist.h"

/**
 * @brief Get a song from a playlist by its 1-based position.
 *
 * Traverses the playlist and returns the node at the specified position.
 * If the playlist is empty, the current song is returned. If the number
 * exceeds the playlist length, the last song is returned.
 *
 * @param playlist Pointer to the playlist.
 * @param song_number 1-based index of the desired song.
 *
 * @return Pointer to the corresponding Node, or the current song if empty.
 */
Node *get_song_by_number(PlayList *playlist, int song_number);

/**
 * @brief Find a playlist entry by its unique identifier.
 *
 * @param playlist Pointer to the playlist.
 * @param id Unique node identifier.
 *
 * @return Pointer to the matching Node, or NULL if not found.
 */
Node *find_selected_entry_by_id(PlayList *playlist, int id);

/**
 * @brief Find a playlist entry by its zero-based row index.
 *
 * @param playlist Pointer to the playlist.
 * @param row Zero-based index in the playlist.
 *
 * @return Pointer to the corresponding Node, or NULL if out of range.
 */
Node *find_selected_entry(PlayList *playlist, int row);

/**
 * @brief Determine the next song to play based on playback state.
 *
 * Uses internal playback flags such as waitingForPlaylist and waitingForNext
 * to determine the appropriate next node.
 *
 * @param playlist Pointer to the active playlist.
 *
 * @return Pointer to the next Node to play, or NULL if none determined.
 */
Node *determine_next_song(PlayList *playlist);

/**
 * @brief Rebuild and preload the specified song as the next track.
 *
 * Initiates asynchronous loading and decoder preparation for the given node.
 *
 * @param song Pointer to the song node to preload.
 */
void rebuild_next_song(Node *song);

/**
 * @brief Prepare playback state for repeating the entire playlist.
 *
 * Sets internal flags so playback restarts from the beginning when the
 * end of the list is reached.
 */
void repeat_list(void);

/**
 * @brief Move a song up one position in the playlist.
 *
 * Updates both shuffled and unshuffled playlists as needed and rebuilds
 * next-song state if affected.
 *
 * @param chosen_row Pointer to the currently selected row index; updated
 *                   to reflect the new position.
 */
void move_song_up(int *chosen_row);

/**
 * @brief Move a song down one position in the playlist.
 *
 * Updates both shuffled and unshuffled playlists as needed and rebuilds
 * next-song state if affected.
 *
 * @param chosen_row Pointer to the currently selected row index; updated
 *                   to reflect the new position.
 */
void move_song_down(int *chosen_row);

/**
 * @brief Handle removal of a song from the playlist view.
 *
 * Removes the selected song depending on the active view and updates
 * playback state and UI accordingly.
 *
 * @param chosen_row Zero-based row index of the selected entry.
 */
void handle_remove(int chosen_row);

/**
 * @brief Skip to a specific numbered song in the playlist.
 *
 * Loads and begins playback of the specified 1-based playlist position.
 * If loading fails, attempts to advance to the next entry.
 *
 * @param song_number 1-based index of the song to play.
 */
void skip_to_numbered_song(int song_number);

/**
 * @brief Remove the currently playing song from the playlist.
 *
 * Stops playback, clears current track state, updates internal flags,
 * and prepares for the next song if available.
 */
void remove_currently_playing_song(void);

/**
 * @brief Skip to the last song in the playlist.
 *
 * Determines the final entry and delegates to skip_to_numbered_song().
 */
void skip_to_last_song(void);

/**
 * @brief Skip to the previous song in the playlist.
 *
 * Handles paused/stopped states, rebuilds decoders if required,
 * and restarts playback from the previous entry.
 */
void skip_to_prev_song(void);

/**
 * @brief Skip to the next song in the playlist.
 *
 * Advances playback to the next entry, respecting shuffle,
 * repeat-list, and paused/stopped states.
 */
void skip_to_next_song(void);

/**
 * @brief Set the current song pointer to the next entry.
 *
 * Updates playback state and tracks the previously played song ID.
 */
void set_current_song_to_next(void);

/**
 * @brief Remove all songs from the playlist except the currently playing one.
 *
 * Updates both shuffled and unshuffled playlists and clears enqueued
 * flags in the library for removed entries.
 */
void dequeue_all_except_playing_song(void);

/**
 * @brief Add the currently playing song to the favorites playlist.
 *
 * If the song is not already present, creates a new node and appends it
 * to the favorites list.
 */
void add_to_favorites_playlist(void);

/**
 * @brief Reshuffle the current playlist.
 *
 * If shuffle mode is enabled, randomizes playlist order while keeping
 * the current song as the starting point when possible.
 */
void reshuffle_playlist(void);

/**
 * @brief Handle skip behavior when playback order was modified.
 *
 * Adjusts current song selection depending on skipOutOfOrder and repeat state.
 */
void handle_skip_out_of_order(void);

/**
 * @brief Play a song from the specified playlist.
 *
 * If paused and the selected song matches the current one, toggles pause.
 * Otherwise prepares and starts playback of the chosen entry.
 *
 * @param playlist Pointer to the playlist containing the selected node.
 */
void playlist_play(PlayList *playlist);

/**
 * @brief Clear current playback state and prepare to play a specific song.
 *
 * Resets playback-related flags and sets the provided node as the next
 * starting point.
 *
 * @param song Pointer to the song node to play.
 */
void clear_and_play(Node *song);

/**
 * @brief Perform preprocessing before starting playback.
 *
 * Checks for end-of-list conditions and returns whether playback
 * previously reached the end.
 *
 * @return true if playback had reached the end of the list, false otherwise.
 */
bool play_pre_processing(void);

/**
 * @brief Perform post-processing after playback begins.
 *
 * Clears temporary flags and handles end-of-list state transitions.
 *
 * @param was_end_of_list Indicates whether playback had reached the end.
 */
void play_post_processing(bool was_end_of_list);

/**
 * @brief Play the favorites playlist.
 *
 * Copies the favorites playlist into the active playlist, shuffles it,
 * and marks corresponding library entries as enqueued.
 */
void play_favorites_playlist(void);

/**
 * @brief Play all songs in the library.
 *
 * Builds a playlist from the entire library, shuffles it, and marks
 * entries as enqueued.
 */
void play_all(void);

/**
 * @brief Play all albums in shuffled album order.
 *
 * Adds shuffled albums from the library into the playlist and marks
 * entries as enqueued.
 */
void play_all_albums(void);

/**
 * @brief Build and play a playlist from command-line arguments.
 *
 * Recursively scans provided paths for supported audio files and adds
 * them to the playlist.
 *
 * @param argc Pointer to argument count.
 * @param argv Argument vector containing file or directory paths.
 */
void play_command_with_playlist(int *argc, char **argv);

#endif
