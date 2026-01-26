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

Node *get_song_by_number(PlayList *playlist, int song_number);
Node *find_selected_entry_by_id(PlayList *playlist, int id);
Node *find_selected_entry(PlayList *playlist, int row);
Node *determine_next_song(PlayList *playlist);
void rebuild_next_song(Node *song);
void repeat_list(void);
void move_song_up(int *chosen_row);
void move_song_down(int *chosen_row);
void handle_remove(int chosen_row);
void skip_to_numbered_song(int song_number);
void remove_currently_playing_song(void);
void skip_to_last_song(void);
void skip_to_prev_song(void);
void skip_to_next_song(void);
void set_current_song_to_next(void);
void dequeue_all_except_playing_song(void);
void add_to_favorites_playlist(void);
void reshuffle_playlist(void);
void handle_skip_out_of_order(void);
void playlist_play(PlayList *playlist);
void clear_and_play(Node *song);
bool play_pre_processing();
void play_post_processing(bool was_end_of_list);
void play_favorites_playlist(void);
void play_all(void);
void play_all_albums(void);
void play_command_with_playlist(int *argc, char **argv);


#endif
