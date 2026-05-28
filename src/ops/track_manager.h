/**
 * @file track_manager.h
 * @brief Central track indexing and metadata manager.
 *
 * Manages track objects, metadata lookup, and synchronization
 * between the library and active playlists. Provides helper
 * functions for finding and referencing track data.
 */

#include "data/playlist_type.h"
#include <stdbool.h>

/**
 * @brief Load a song into a decoder buffer.
 *
 * Initiates loading of the specified song using the provided loading
 * context. Depending on the loading flags, this may load either the
 * primary or secondary decoder buffer.
 *
 * @param song          Pointer to the song node to load.
 * @param is_first_decoder Whether to start a new decoder chain.
 * @param replace_next_song Whether to replace the song in the next slot.
 */

void load_song(Node *song, bool is_first_decoder, bool replace_next_song);

/**
 * @brief Load the next song in the playlist.
 *
 * Prepares and initiates loading of the next song relative to the
 * current playback position. Selects the alternate decoder buffer
 * and updates playback state accordingly.
 * @param replace_next_song Whether to replace the song in the next slot.
 */
void load_next_song(bool replace_next_song);

/**
 * @brief Finalize loading of the next song.
 *
 * Waits for the asynchronous loading process to complete or until
 * a timeout is reached. Marks the next song as loaded once the
 * operation completes.
 */
void finish_loading(void);

/**
 * @brief Unload song data from decoder buffer A.
 *
 * Frees resources associated with buffer A if it has not already
 * been deleted and clears its corresponding user data pointer.
 */
void unload_song_a(void);

/**
 * @brief Unload song data from decoder buffer B.
 *
 * Frees resources associated with buffer B if it has not already
 * been deleted and clears its corresponding user data pointer.
 */
void unload_song_b(void);

/**
 * @brief Automatically prepare playback if currently stopped.
 *
 * If a valid file path is provided, locates the corresponding song
 * in the playlist and sets it as the next starting point without
 * immediately beginning playback.
 *
 * @param path File system path of the song to autostart.
 */
void autostart_if_stopped(const char *path);

/**
 * @brief Load and prepare the first playable song.
 *
 * Attempts to load the specified song into the primary decoder buffer.
 * If loading fails due to errors, iterates through subsequent songs
 * until a playable one is found or the list is exhausted.
 *
 * Initializes playback state and PCM frame counters upon success.
 *
 * @param song Pointer to the initial song node to attempt loading.
 *
 * @return 0 on success, -1 if no playable song could be loaded.
 */
int load_first(Node *song);

/**
 * @brief Checks and loads the next song if necessary.
 *
 * This function checks if a song needs to be loaded, either due to a restart or if the
 * playlist is in shuffle mode. It prepares and loads the next song in the playlist if needed.
 *
 * @param seconds How far into the song to start playing.
 */
void check_and_load_next_song(double seconds);

/**
 * @brief Loads the next song if needed, considering the current state of the playlist and player.
 *
 * This function loads the next song from the playlist if the current song has finished or
 * if the player is waiting for the next song. It handles various conditions, including
 * handling end-of-list behavior and errors.
 */
 void load_waiting_music(void);

/**
 * @brief Determines the current song and sends a notification if needed.
 *
 * This function checks the current song data and updates its duration. It also
 * checks if the song has changed since the last notification and, if so, sends
 * a notification about the song switch.
 */
void determine_song_and_notify(void);

