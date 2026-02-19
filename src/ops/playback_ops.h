/**
 * @file playback_ops.h
 * @brief Core playback control API.
 *
 * Contains functions to control playback: play, pause, stop, seek,
 * volume adjustments, and track skipping. This module is UI-agnostic
 * and interacts directly with the playback state and audio backends.
 */

#ifndef PLAYBACK_OPS_H
#define PLAYBACK_OPS_H

#include "common/appstate.h"

/**
 * @brief Start playback of the specified song node.
 *
 * Validates the given node, prepares decoder state, and asynchronously loads
 * the associated audio data. If loading succeeds, resets the playback clock
 * and initiates playback. If loading fails and a next node exists, a skip
 * may be triggered.
 *
 * @param node Pointer to the playlist node representing the song to play.
 *
 * @return 0 on success, -1 if the node is invalid or playback could not start.
 */
int play_song(Node *node);

/**
 * @brief Pause the currently playing song.
 *
 * If playback is not already paused, updates the pause timestamp and emits
 * a "Paused" playback status signal before pausing the audio backend.
 */
void pause_song(void);

/**
 * @brief Resume or start playback.
 *
 * If paused, resumes playback and restores accumulated pause time.
 * If stopped, starts playback from the beginning of the current track.
 * Emits a "Playing" playback status signal.
 */
void play(void);

/**
 * @brief Stop playback and reset position.
 *
 * Stops the audio backend. If successfully stopped, resets playback
 * position to the beginning of the current track and emits a "Stopped"
 * playback status signal.
 */
void stop(void);

/**
 * @brief Toggle between paused and playing states.
 *
 * If stopped, resets the playback clock. Toggles the pause state in the
 * audio backend and updates pause timing and playback status signals
 * accordingly.
 */
void ops_toggle_pause(void);

/**
 * @brief Resume playback from a paused state.
 *
 * Calls the underlying audio backend to resume playback without modifying
 * clock or pause accounting state.
 */
void resume_playback(void);

/**
 * @brief Adjust the playback volume.
 *
 * Modifies the output volume by the specified percentage delta.
 *
 * @param change_percent Percentage change to apply to the current volume.
 *                       Positive values increase volume, negative values
 *                       decrease it.
 */
void volume_change(int change_percent);

/**
 * @brief Seek relative to the current playback position.
 *
 * Adds a relative offset in seconds to the internal seek accumulator.
 * Has no effect if playback is paused, no song is loaded, the duration
 * is invalid, or the current format does not support seeking.
 *
 * @param seconds Number of seconds to seek forward (positive) or backward
 *                (negative).
 */
void seek(int seconds);

/**
 * @brief Skip to a specific song in the playlist.
 *
 * Locates a song by its unique identifier in the active playlist and
 * initiates playback via play_song(). Optionally ensures playback is
 * started before switching tracks.
 *
 * If the player is currently loading or skipping and not forced to skip,
 * the request is ignored.
 *
 * @param id Unique identifier of the target song node.
 * @param start_playing If true, ensures playback is active before skipping.
 */
void skip_to_song(int id, bool start_playing);

#endif
