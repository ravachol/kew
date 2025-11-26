/**
 * @file track_manager.h
 * @brief Central track indexing and metadata manager.
 *
 * Manages track objects, metadata lookup, and synchronization
 * between the library and active playlists. Provides helper
 * functions for finding and referencing track data.
 */

#include "common/appstate.h"

void load_song(Node *song, LoadingThreadData *loadingdata);
void load_next_song(void);
void finish_loading(void);
void unload_song_a(void);
void unload_song_b(void);
void unload_previous_song(void);
void try_load_next(void);
void autostart_if_stopped(const char *path);
int load_first(Node *song);
