/**
 * @file sound_facade.c
 * @brief Public facade interface for the audio subsystem.
 *
 * This header defines the high-level API for interacting with the audio
 * subsystem. It provides a simplified facade over the underlying audio
 * implementation, abstracting decoder management, device handling,
 * playback control, and configuration.
 *
 * Clients should include this header and use the exposed functions to
 * create, control, and destroy the sound system without needing knowledge
 * of the internal audio architecture.
 *
 * IMPORTANT: This is the only sound header file that should be included outside of the sound module.
 *            The sound module shouldn't use any of the other modules except for loader and utils modules.
 */

#ifndef SOUND_FACADE_H
#define SOUND_FACADE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "audiotypes.h"

#include <stddef.h>
#include <stdint.h>

/*
 * Public error codes for the audio subsystem.
 * Keep these high-level and implementation-agnostic.
 */
typedef enum {
        SOUND_OK = 0,
        SOUND_ERROR_INVALID_ARGUMENT,
        SOUND_ERROR_NOT_INITIALIZED,
        SOUND_ERROR_ALREADY_INITIALIZED,
        SOUND_ERROR_BACKEND_FAILURE,
        SOUND_ERROR_UNSUPPORTED_FORMAT,
        SOUND_ERROR_IO,
        SOUND_ERROR_INTERNAL
} sound_result_t;

/*
 * Opaque handle to the audio system.
 * Implementation details are hidden in the .c file.
 */
typedef struct sound_system sound_system_t;

/*=========================================================
  Lifecycle
=========================================================*/

/**
 * @brief Creates and initializes the sound system.
 *
 * Allocates and initializes all resources required for audio playback.
 * This function must be called before any other sound system function.
 *
 * @param out_system Pointer to a sound_system_t pointer that will receive
 *                   the newly created instance on success.
 * @return sound_result_t Result code indicating success or failure.
 */
sound_result_t sound_system_create(sound_system_t **out_system);

/**
 * @brief Destroys the sound system and releases resources.
 *
 * Shuts down the audio system and frees all associated resources.
 *
 * @param system Double Pointer to the sound system instance to destroy.
 */
void sound_system_destroy(sound_system_t **system);

/**
 * @brief Start switching the decoder.
 *
 * Called when a new song needs to load of a different type than the currently playing one.
 *
 * @param system Double Pointer to the sound system instance to destroy.
 * @return sound_result_t Result code indicating success or failure.
 */
sound_result_t sound_system_switch_decoder(sound_system_t *system);

/**
 * @brief Uninitializes the audio device.
 *
 * Releases resources associated with the audio device and performs
 * any necessary cleanup for the provided sound system instance.
 *
 * @param system Pointer to the sound system instance.
 * @return sound_result_t Result code indicating success or failure.
 */
sound_result_t sound_system_uninit_device(sound_system_t *system);

/**
 * @brief Starts a new decoder chain.
 *
 * Sets the audio implementation type to NONE.
 * This prepares the sound system for initializing a new decoder chain.
 * Needed for instance when switch track prematurely.
 */
void sound_system_start_new_decoder_chain(void);

/*=========================================================
  Playback Control
=========================================================*/

/**
 * @brief Loads an audio file.
 *
 * Loads the specified audio file into the sound system. This function
 * does not automatically start playback.
 *
 * @param system Pointer to the sound system instance.
 * @param file_path Path to the audio file to load.
 * @param is_first_decoder Whether to start a new decoder chain.
 * @param is_next_song Whether to load this song in the next slot
 * @return sound_result_t Result code indicating success or failure.
 */
sound_result_t sound_system_load(
    sound_system_t *system,
    const char *file_path,
    int is_first_decoder,
    int is_next_song);

/**
 * @brief Unloads all songs currently managed by the sound system.
 *
 * Frees resources associated with both the current and any preloaded
 * songs without shutting down the audio device itself.
 *
 * After this call, no songs remain loaded in the system, but the
 * sound system may still be initialized and ready to load new tracks.
 *
 * @param system Pointer to the sound system instance.
 *
 * @return sound_result_t Status code indicating success or failure
 *                        of the unload operation.
 *
 * @warning This function should not be called while audio processing
 *          threads are actively accessing song data.
 */
sound_result_t sound_system_unload_songs(sound_system_t *system);

/**
 * @brief Starts or resumes playback.
 *
 * Begins playback of the currently loaded audio track, or resumes
 * playback if it is paused.
 *
 * @param system Pointer to the sound system instance.
 * @return sound_result_t Result code indicating success or failure.
 */
sound_result_t sound_system_play(sound_system_t *system);

/**
 * @brief Pauses playback.
 *
 * Pauses playback of the currently loaded audio track.
 *
 * @param system Pointer to the sound system instance.
 * @return sound_result_t Result code indicating success or failure.
 */
sound_result_t sound_system_pause(sound_system_t *system);

/**
 * @brief Stop playback and reset position to beginning.
 *
 * @param system Pointer to the sound system instance.
 * @return sound_result_t Result code indicating success or failure.
 */
sound_result_t sound_system_stop(sound_system_t *system);

/**
 * @brief Stops the active audio decoding process.
 *
 * Signals the sound system to stop decoding the currently processed
 * audio stream. This does not necessarily unload the track or shut
 * down the audio device, but ensures that no further audio data is
 * decoded until decoding is restarted.
 *
 * @param system Pointer to the sound system instance.
 *
 * @return sound_result_t Status code indicating success or failure
 *                        of the stop operation.
 *
 * @note Safe usage may require external synchronization if decoding
 *       occurs on a separate thread.
 */
sound_result_t sound_system_stop_decoding(sound_system_t *system);

/**
 * @brief Skips playback to the next loaded song.
 *
 * Requests a transition from the current track to the next queued
 * or preloaded track. The transition behavior (e.g., smooth, buffered,
 * or immediate) depends on the internal sound system implementation.
 *
 * @param system Pointer to the sound system instance.
 *
 * @return sound_result_t Status code indicating whether the skip
 *                        request was successfully processed.
 *
 * @note A next track must already be loaded for this operation to succeed.
 */
sound_result_t sound_system_skip_to_next(sound_system_t *system);

/**
 * @brief Immediately switches to the next song without transition.
 *
 * Forces an immediate change from the current track to the next one,
 * bypassing any fade-out, buffering, or transition logic.
 *
 * This function is typically used for abrupt track changes such as
 * user-initiated skips.
 *
 * @param system Pointer to the sound system instance.
 *
 * @return sound_result_t Status code indicating success or failure.
 *
 * @warning May interrupt active decoding or playback threads and
 *          should be used with proper synchronization.
 */
sound_result_t sound_system_switch_song_immediate(sound_system_t *system);

/**
 * @brief Clears the currently loaded track from the sound system.
 *
 * Unloads and resets the active track without affecting other queued
 * or preloaded tracks (if supported by the implementation).
 *
 * After calling this function, no current track will be available
 * for playback until a new one is loaded.
 *
 * @param system Pointer to the sound system instance.
 *
 * @return sound_result_t Status code indicating success or failure
 *                        of the clear operation.
 */
sound_result_t sound_system_clear_current_track(sound_system_t *system);

/**
 * @brief Toggles the pause state of the sound system.
 *
 * If the system is currently playing, it will be paused. If it is paused,
 * playback will resume.
 *
 * @param system Pointer to the sound system instance.
 * @return sound_result_t Result code indicating success or failure.
 */
sound_result_t sound_system_toggle_pause(sound_system_t *system);

/**
 * @brief Seeks to a position within the track by percentage.
 *
 * Moves playback to the specified percentage of the total track length.
 *
 * @param system Pointer to the sound system instance.
 * @param percent Target position as a percentage (typically 0.0f to 100.0f).
 * @return sound_result_t Result code indicating success or failure.
 */
sound_result_t sound_system_seek_percentage(sound_system_t *system, float percent);

/**
 * @brief Sets the total accumulated seek time.
 *
 * Updates the internal seek elapsed time, expressed in seconds.
 *
 * @param seek_elapsed The total seek time accumulated, in seconds.
 * @return sound_result_t Result code indicating success or failure.
 */
sound_result_t sound_system_set_seek_elapsed(double seek_elapsed);

/**
 * @brief Sets the repeat state.
 *
 * Updates the repeat mode of the sound system.
 *
 * @param value The repeat state to set.
 * @return sound_result_t Result code indicating success or failure.
 */
sound_result_t sound_system_set_repeat_state(int value);

/**
 * @brief Signals that the EOF status has been handled.
 *
 * @param system Pointer to the sound system instance.
 * @return sound_result_t Result code indicating success or failure.
 */
sound_result_t sound_system_set_EOF_handled(const sound_system_t *system);

/**
 * @brief Sets whether the end of the playlist has been reached.
 *
 * Updates the internal flag indicating that playback has reached
 * the end of the available track list.
 *
 * @param system Pointer to the sound system instance.
 * @param value  Non-zero to indicate end-of-list reached, 0 otherwise.
 *
 * @return sound_result_t Status code indicating success or failure.
 */
sound_result_t sound_system_set_end_of_list_reached(sound_system_t *system, int value);

/**
 * @brief Sets whether replay gain should check track gain before album gain.
 *
 * Configures the replay gain evaluation order. When enabled, the
 * system prioritizes per-track replay gain values over album-level
 * gain values (if both are available).
 *
 * @param system Pointer to the sound system instance.
 * @param value  Non-zero to check track gain first, 0 to prefer album gain.
 *
 * @return sound_result_t Status code indicating success or failure.
 */
sound_result_t sound_system_set_replay_gain_check_track_first(sound_system_t *system, int value);

/**
 * @brief Sets whether the output audio buffer is ready.
 *
 * This should be set to false when the buffer is ready and it's going to be read.
 *
 * @param system Pointer to the sound system instance.
 * @param value  Non-zero to indicate if the buffer is ready, 0 otherwise.
 *
 * @return sound_result_t Status code indicating success or failure.
 */
sound_result_t sound_system_set_buffer_ready(const sound_system_t *system, int value);

/*=========================================================
  State Queries
=========================================================*/

/**
 * @brief Retrieves the current playback state.
 *
 * Returns the current state of the sound system (e.g., playing,
 * paused, stopped).
 *
 * @param system Pointer to the sound system instance.
 * @return sound_playback_state_t The current playback state.
 */
sound_playback_state_t sound_system_get_state(
    const sound_system_t *system);

/**
 * @brief Gets the duration of the currently loaded track.
 *
 * Returns the total length of the loaded audio track in milliseconds.
 *
 * @param system Pointer to the sound system instance.
 * @return uint64_t Track duration in milliseconds.
 */
uint64_t sound_system_get_duration_ms(
    const sound_system_t *system);

/**
 * @brief Retrieves the current repeat state.
 *
 * Returns the repeat mode currently configured in the sound system.
 *
 * @return int The current repeat state value.
 */
int sound_system_get_repeat_state(void);

/**
 * @brief Gets the total accumulated seek time.
 *
 * Returns the internal seek elapsed time in seconds.
 *
 * @return double The total accumulated seek time, in seconds.
 */
double sound_system_get_seek_elapsed(void);

/**
 * @brief Returns whether the system is currently switching tracks.
 *
 * Indicates if a track transition is in progress, such as during
 * a skip or immediate switch operation.
 *
 * @param system Pointer to the sound system instance.
 *
 * @return Non-zero if a track switch is currently in progress,
 *         0 otherwise.
 */
int sound_system_is_switching_track(const sound_system_t *system);

/**
 * @brief Returns whether end-of-file (EOF) has been reached.
 *
 * Indicates that the currently playing track has reached the end
 * of its decoded audio stream.
 *
 * @return Non-zero if EOF has been reached, 0 otherwise.
 *
 * @note This function does not require a system pointer and may
 *       reflect a global or shared playback state.
 */
int sound_system_is_EOF_reached(void);

/**
 * @brief Returns whether the current song has been deleted.
 *
 * Indicates if the active SongData associated with the system
 * has been marked as deleted or released.
 *
 * @param system Pointer to the sound system instance.
 *
 * @return Non-zero if the current song is deleted, 0 otherwise.
 */
int sound_system_is_current_song_deleted(const sound_system_t *system);

/**
 * @brief Returns whether no song is currently loaded.
 *
 * Checks if the sound system has no active track available
 * for playback.
 *
 * @param system Pointer to the sound system instance.
 *
 * @return Non-zero if no song is loaded, 0 otherwise.
 */
int sound_system_no_song_loaded(const sound_system_t *system);

/**
 * @brief Returns whether the end of the playlist has been reached.
 *
 * Retrieves the internal end-of-list flag.
 *
 * @param system Pointer to the sound system instance.
 *
 * @return Non-zero if the end of the track list has been reached,
 *         0 otherwise.
 */
int sound_system_is_end_of_list_reached(const sound_system_t *system);

/**
 * @brief Returns the currently active SongData.
 *
 * Provides access to the SongData structure representing the
 * currently loaded or playing track.
 *
 * @param system Pointer to the sound system instance.
 *
 * @return Pointer to the current SongData, or NULL if no song
 *         is loaded.
 *
 * @warning The returned pointer is owned by the sound system and
 *          must not be freed by the caller.
 */
SongData *sound_system_get_current_song(const sound_system_t *system);

/**
 * @brief Returns whether replay gain checks track gain first.
 *
 * Retrieves the current configuration for replay gain priority.
 *
 * @param system Pointer to the sound system instance.
 *
 * @return Non-zero if track gain is prioritized, 0 if album gain
 *         is prioritized.
 */
int sound_system_get_replay_gain_check_track_first(const sound_system_t *system);

/**
 * @brief Returns the output sample rate of the sound system.
 *
 * Provides the configured or active audio sample rate used
 * for playback (e.g., 44100 Hz, 48000 Hz).
 *
 * @param system Pointer to the sound system instance.
 *
 * @return The current sample rate in Hz.
 */
int sound_system_get_sample_rate(const sound_system_t *system);

/**
 * @brief Returns the bit depth of the currently playing song
 *
 * @param system Pointer to the sound system instance.
 *
 * @return The current bit depth.
 */
int sound_system_get_bit_depth(const sound_system_t *system);

/**
 * @brief Returns the current output audio buffer
 *
 * Returns a buffer with audio samples in the type that corresponds for the current format, for instance if the format is
 * f32 it will return floats.
 *
 * @param system Pointer to the sound system instance.
 *
 * @return The current bit depth.
 */
void *sound_system_get_audio_buffer(const sound_system_t *system);

/**
 * @brief Gets the FFT size.
 *
 * This function returns the current FFT size used for processing audio data.
 *
 * @param system Pointer to the sound system instance.
 *
 * @return The current FFT size in samples.
 */
int sound_system_get_fft_size(const sound_system_t *system);

/**
 * @brief Returns true if the buffer is read to be read.
 *
 * @param system Pointer to the sound system instance.
 *
 * @return Whether the audio buffer is ready to be read from.
 */
int sound_system_is_buffer_ready(const sound_system_t *system);

/*=========================================================
  Audio Settings
=========================================================*/

/**
 * @brief Sets the master volume.
 *
 * Sets the output volume for the sound system.
 *
 * @param system Pointer to the sound system instance.
 * @param volume Master volume level in the range 0.0f (silent) to 1.0f (maximum).
 * @return sound_result_t Result code indicating success or failure.
 */
sound_result_t sound_system_set_volume(
    sound_system_t *system,
    float volume);

/**
 * @brief Retrieves the master volume.
 *
 * Gets the current output volume of the sound system.
 *
 * @param system Pointer to the sound system instance.
 * @return float Current master volume in the range 0.0f to 1.0f.
 */
float sound_system_get_volume(
    const sound_system_t *system);

#ifdef __cplusplus
}
#endif

#endif /* SOUND_FACADE_H */
