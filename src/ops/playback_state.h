/**
 * @file playback_state.h
 * @brief Maintains playback runtime state.
 *
 * Stores and manages the current playback status, elapsed time,
 * current song pointer, and related flags (paused, stopped, etc.).
 * Provides accessors and mutators for playback state data.
 */

#include "common/appstate.h"

#include <stdbool.h>

// Getters
bool is_repeat_list_enabled(void);
bool is_shuffle_enabled(void);
bool ops_is_repeat_enabled(void);
bool ops_is_paused(void);
bool ops_is_stopped(void);
bool ops_is_done(void);
bool ops_is_EOF(void);
bool is_current_song_deleted(void);
bool ops_is_impl_switch_reached(void);
bool determine_current_song_data(SongData **currentSongData);
int get_volume();
double get_current_song_duration(void);
void get_format_and_sample_rate(ma_format *format, ma_uint32 *sample_rate);
SongData *get_current_song_data(void);

// Setters
void set_repeat_enabled(bool enabled);
void set_repeat_list_enabled(bool value);
void set_shuffle_enabled(bool value);
void ops_set_EOF_handled(void);

