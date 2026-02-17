/**
 * @file track_manager.h
 * @brief Central track indexing and metadata manager.
 *
 * Manages track objects, metadata lookup, and synchronization
 * between the library and active playlists. Provides helper
 * functions for finding and referencing track data.
 */

#include "common/appstate.h"

/**
 * @brief Load a song into a decoder buffer.
 *
 * Initiates loading of the specified song using the provided loading
 * context. Depending on the loading flags, this may load either the
 * primary or secondary decoder buffer.
 *
 * @param song         Pointer to the song node to load.
 * @param loadingdata  Pointer to the loading thread data structure
 *                     containing decoder and state information.
 */
void load_song(Node *song, LoadingThreadData *loadingdata);

/**
 * @brief Load the next song in the playlist.
 *
 * Prepares and initiates loading of the next song relative to the
 * current playback position. Selects the alternate decoder buffer
 * and updates playback state accordingly.
 */
void load_next_song(void);

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
 * @brief Unload the previously used song buffer.
 *
 * Determines which decoder buffer (A or B) is no longer needed and
 * safely unloads it while holding the required synchronization locks.
 * Updates playback state to reflect buffer switching.
 */
void unload_previous_song(void);

/**
 * @brief Attempt to load the next song if required.
 *
 * Triggers loading of the next track when playback state indicates
 * that a new song should be prepared. Typically used in conjunction
 * with preloading and seamless transitions.
 */
void try_load_next(void);

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
