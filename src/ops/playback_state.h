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

/* =========================
 * Getters
 * ========================= */

/**
 * @brief Check whether repeat-list mode is enabled.
 *
 * When enabled, reaching the end of the playlist causes playback
 * to restart from the beginning of the list.
 *
 * @return true if repeat-list mode is active, false otherwise.
 */
bool is_repeat_list_enabled(void);

/**
 * @brief Check whether shuffle mode is enabled.
 *
 * @return true if shuffle mode is active, false otherwise.
 */
bool is_shuffle_enabled(void);

/**
 * @brief Check whether single-track repeat is enabled.
 *
 * @return true if repeating the current track is enabled, false otherwise.
 */
bool is_repeat_enabled(void);

/**
 * @brief Check whether playback is currently paused.
 *
 * @return true if playback is paused, false otherwise.
 */
bool is_paused(void);

/**
 * @brief Check whether playback is currently stopped.
 *
 * @return true if playback is stopped, false otherwise.
 */
bool is_stopped(void);

/**
 * @brief Check whether playback of the current track has completed.
 *
 * @return true if playback is finished, false otherwise.
 */
bool is_playback_done(void);

/**
 * @brief Check whether the end-of-file (EOF) condition has been reached.
 *
 * Indicates that the audio backend has reached the end of the current stream.
 *
 * @return true if EOF has been reached, false otherwise.
 */
bool is_EOF_reached(void);

/**
 * @brief Check whether the currently active SongData buffer has been deleted.
 *
 * Determines whether the active audio buffer (A or B) is marked as deleted
 * in the double-buffered playback system.
 *
 * @return true if the current song data buffer is deleted, false otherwise.
 */
bool is_current_song_deleted(void);

/**
 * @brief Check whether an implementation switch point has been reached.
 *
 * Used internally to determine whether the playback backend should switch
 * between double-buffered audio data implementations.
 *
 * @return true if an implementation switch has been reached, false otherwise.
 */
bool is_impl_switch_reached(void);

/**
 * @brief Determine the currently active SongData pointer.
 *
 * Selects the active SongData buffer (A or B) depending on the current file
 * index. If the active buffer is deleted, attempts to switch to the alternate
 * buffer when possible.
 *
 * @param[out] current_song_data Pointer that receives the active SongData.
 *
 * @return true if the selected SongData is deleted, false otherwise.
 */
bool determine_current_song_data(SongData **current_song_data);

/**
 * @brief Get the current playback volume.
 *
 * @return Current volume level as an integer percentage or backend-defined scale.
 */
int get_volume(void);

/**
 * @brief Get the duration of the currently loaded song.
 *
 * Retrieves the duration from the active SongData structure.
 *
 * @return Duration of the current song in seconds, or 0.0 if unavailable.
 */
double get_current_song_duration(void);

/**
 * @brief Retrieve the current audio output format and sample rate.
 *
 * Queries the audio backend for the active output format and sample rate.
 *
 * @param[out] format Pointer that receives the current sample format.
 * @param[out] sample_rate Pointer that receives the current sample rate (Hz).
 */
void get_format_and_sample_rate(ma_format *format, ma_uint32 *sample_rate);

/**
 * @brief Get the currently active SongData structure.
 *
 * Returns the validated SongData associated with the current playlist entry.
 * Ensures the data is not deleted and contains valid metadata.
 *
 * @return Pointer to the active SongData, or NULL if unavailable or invalid.
 */
SongData *get_current_song_data(void);


/* =========================
 * Setters
 * ========================= */

/**
 * @brief Enable or disable single-track repeat mode.
 *
 * @param enabled true to enable repeat of the current track, false to disable.
 */
void set_repeat_enabled(bool enabled);

/**
 * @brief Enable or disable repeat-list mode.
 *
 * @param value true to enable repeating the entire playlist,
 *              false to disable.
 */
void set_repeat_list_enabled(bool value);

/**
 * @brief Enable or disable shuffle mode.
 *
 * @param value true to enable shuffle playback, false to disable.
 */
void set_shuffle_enabled(bool value);

/**
 * @brief Mark the EOF condition as handled.
 *
 * Notifies the playback backend that the end-of-file state has been processed,
 * preventing repeated handling of the same EOF event.
 */
void set_EOF_handled(void);

