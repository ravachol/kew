/**
 * @file queue_ui.h
 * @brief Handles high level enqueue
 *
 */

#include "common/appstate.h"

/**
 * @brief Determines the current song and sends a notification if needed.
 *
 * This function checks the current song data and updates its duration. It also
 * checks if the song has changed since the last notification and, if so, sends
 * a notification about the song switch.
 */
void determine_song_and_notify(void);

/**
 * @brief Resets the playlist and playback state after dequeuing the currently playing song.
 *
 * This function resets various application states related to the playlist and playback
 * when the currently playing song is dequeued, ensuring the playback state is properly cleaned up.
 */
void reset_list_after_dequeuing_playing_song(void);

/**
 * @brief Enqueues a song and optionally starts playing it immediately.
 *
 * This function enqueues a song in the playlist, and if the `play_immediately` flag
 * is set to true, it starts playing the song right away.
 *
 * @param play_immediately Flag to indicate whether to start playing the song immediately.
 */
void view_enqueue(bool play_immediately);

/**
 * @brief Handles the action of jumping to a specific song in the playlist.
 *
 * This function performs the necessary steps to navigate to a specific song when
 * the "Go To Song" action is triggered.
 */
void handleGoToSong(void);

/**
 * @brief Updates the next song to be played if needed.
 *
 * This function checks if the next song in the playlist should be updated and
 * performs the necessary updates to the playback state.
 */
void update_next_song_if_needed(void);

/**
 * @brief Enqueues a single song entry into the playlist.
 *
 * This function adds a single song entry to the playlist and returns the entry that was enqueued.
 *
 * @param entry The song entry to enqueue.
 * @return The first enqueued entry (if any).
 */
FileSystemEntry *enqueue(FileSystemEntry *entry);

/**
 * @brief Enqueues a set of songs from a directory into the playlist.
 *
 * This function enqueues all songs from a given directory into the playlist, ensuring
 * any necessary directory structures are handled.
 *
 * @param entry The directory entry to enqueue.
 * @param chosen_dir Pointer to the chosen directory, which can be updated.
 * @return The first enqueued entry (if any).
 */
FileSystemEntry *enqueue_songs(FileSystemEntry *entry, FileSystemEntry **chosen_dir);

/**
 * @brief Enqueues a playlist from a given playlist entry.
 *
 * This function enqueues songs from a playlist and returns the first enqueued entry.
 *
 * @param playlist The playlist containing the songs to enqueue.
 * @return The first enqueued entry (if any).
 */
FileSystemEntry *libraryEnqueue(PlayList *playlist);
