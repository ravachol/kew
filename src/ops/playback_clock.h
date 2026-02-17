/**
 * @file playback_clock.h
 * @brief Playback timing and synchronization utilities.
 *
 * Handles timing measurements for song progress, seek operations,
 * and playback duration calculations. Uses system timers or
 * monotonic clocks to maintain precise playback timing.
 */

#ifndef PLAYBACK_CLOCK_H
#define PLAYBACK_CLOCK_H

#include <glib.h>
#include <stdbool.h>
#include <time.h>

/**
 * @brief Reset the playback clock state.
 *
 * Resets elapsed time, pause time, accumulated seek offsets, and total
 * paused duration. Also updates the internal start timestamp using
 * CLOCK_MONOTONIC.
 */
void reset_clock(void);

/**
 * @brief Recalculate the elapsed playback time.
 *
 * Updates the internally tracked elapsed playback time based on the
 * monotonic clock, current seek offsets, and accumulated pause duration.
 * If playback is paused, updates the current pause duration instead.
 *
 * The computed elapsed time is clamped to the range [0, duration].
 *
 * @param duration Total duration of the currently playing track in seconds.
 */
void calc_elapsed_time(double duration);

/**
 * @brief Record the current time as the pause start time.
 *
 * Captures the current CLOCK_MONOTONIC timestamp to mark when playback
 * was paused. Used to calculate accumulated paused duration.
 */
void update_pause_time(void);

/**
 * @brief Add a value to the accumulated seek offset.
 *
 * Adjusts the internal seek accumulator by the specified number of seconds.
 * This offset will be applied when flushing the seek operation.
 *
 * @param value Number of seconds to add to the accumulated seek offset.
 */
void add_to_accumulated_seconds(double value);

/**
 * @brief Set the absolute playback position.
 *
 * Calculates the difference between the requested position and the current
 * playback position, and updates the internal seek accumulator accordingly.
 * Has no effect if playback is paused or duration is zero.
 *
 * @param new_position Target position in microseconds.
 * @param duration Total duration of the current track in seconds.
 *
 * @return true if the position change was accepted, false otherwise.
 */
bool set_position(gint64 new_position, double duration);

/**
 * @brief Seek playback by a relative offset.
 *
 * Adds a relative offset (in microseconds) to the accumulated seek amount.
 * Has no effect if playback is paused or duration is zero.
 *
 * @param offset Relative offset in microseconds.
 * @param duration Total duration of the current track in seconds.
 *
 * @return true if the seek request was accepted, false otherwise.
 */
bool seek_position(gint64 offset, double duration);

/**
 * @brief Apply any pending seek operation.
 *
 * If a seek offset has been accumulated, updates the internal seek state,
 * recalculates elapsed time, emits a seek notification, and resets the
 * accumulator. May reject certain formats depending on decoder constraints.
 *
 * @return true if a seek was applied, false if no seek was pending or
 *         if the operation could not be performed.
 */
bool flush_seek(void);

/**
 * @brief Get the current elapsed playback time.
 *
 * @return Elapsed playback time in seconds.
 */
double get_elapsed_seconds(void);

/**
 * @brief Get the timestamp at which playback was paused.
 *
 * @return The stored pause timestamp as a struct timespec.
 */
struct timespec get_pause_time(void);

/**
 * @brief Get the duration of the currently playing track.
 *
 * Retrieves the total duration of the active track.
 *
 * @return Duration of the current track in seconds.
 */
double get_current_song_duration(void);

#endif
