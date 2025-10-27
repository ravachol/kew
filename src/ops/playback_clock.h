/**
 * @file playback_clock.h
 * @brief Playback timing and synchronization utilities.
 *
 * Handles timing measurements for song progress, seek operations,
 * and playback duration calculations. Uses system timers or
 * monotonic clocks to maintain precise playback timing.
 */

#include <time.h>
#include <glib.h>
#include <stdbool.h>

void reset_clock(void);
void calc_elapsed_time(double duration);
void update_pause_time(void);
void add_to_accumulated_seconds(double value);
bool set_position(gint64 newPosition, double duration);
bool seek_position(gint64 offset, double duration);
bool flush_seek(void);
double get_elapsed_seconds(void);
struct timespec get_pause_time(void);
double get_current_song_duration(void);
