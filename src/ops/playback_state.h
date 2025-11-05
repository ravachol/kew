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
bool is_repeat_enabled(void);
bool is_paused(void);
bool is_stopped(void);
bool is_playback_done(void);
bool is_EOF_reached(void);
bool is_current_song_deleted(void);
bool is_impl_switch_reached(void);
bool determine_current_song_data(SongData **current_song_data);
int get_volume();
double get_current_song_duration(void);
void get_format_and_sample_rate(ma_format *format, ma_uint32 *sample_rate);
SongData *get_current_song_data(void);

// Setters
void set_repeat_enabled(bool enabled);
void set_repeat_list_enabled(bool value);
void set_shuffle_enabled(bool value);
void set_EOF_handled(void);
