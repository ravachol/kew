/**
 * @file sound.h
 * @brief High-level audio playback interface.
 *
 * Provides a unified API for creating an audio device
 * and switching decoders.
 */

#ifndef SOUND_H
#define SOUND_H

#include <miniaudio.h>

#include "sound_facade.h"

#include <stdatomic.h>
#include <stdbool.h>

extern sound_system_t *sound_s;
extern sound_playback_repeat_state_t repeat_state;

/**
 * @brief Checks if the audio context has been initialized.
 *
 * This function returns the current state of the audio context, indicating whether
 * the context has been initialized or not.
 *
 * @return True if the context is initialized, otherwise false.
 */
bool is_context_initialized(void);

/**
 * @brief Creates and initializes the audio playback device.
 *
 * This function initializes the audio context and creates a new playback device.
 * If an audio context already exists, it is uninitialized before reinitializing it.
 * The function also attempts to switch to a new decoder type.
 *
 * @return 0 on success, or -1 if an error occurs while switching the decoder type.
 */
int sound_create_audio_device(void);

/**
 * @brief Switches the decoder type for playback.
 *
 * This function handles the process of switching to a different decoder type,
 * including loading the necessary song data and handling codec operations. It checks
 * whether the end of the audio list has been reached and attempts to switch to the appropriate codec.
 *
 * @return 0 if successful, or -1 if there is an error in switching the type.
 */
int sound_switch_decoder_type(void);

/**
 * @brief Cleans up and uninitializes the audio context.
 *
 * This function releases the audio context and sets the initialization flag to false,
 * effectively cleaning up the resources associated with the audio context.
 */
void cleanup_audio_context(void);

/**
 * @brief Loads a song for decoding and playback.
 *
 * Initiates loading of an audio file into the decoding pipeline.
 * The behavior depends on whether this is the first decoder instance
 * and whether the song is intended to be the next track.
 *
 * @param file_path        Path to the audio file to load.
 * @param is_first_decoder Non-zero if this decoder instance is the first
 *                         (primary) decoder; 0 otherwise.
 * @param is_next_song     Non-zero if the file should be prepared as the
 *                         next track; 0 if it should replace the current one.
 *
 * @note This function may trigger asynchronous decoding depending on
 *       the system design.
 */
void sound_load_song(const char *file_path,
                     int is_first_decoder,
                     int is_next_song);

/**
 * @brief Returns the currently active SongData.
 *
 * Provides access to the SongData structure representing the
 * current track.
 *
 * @return Pointer to the current SongData, or NULL if no song
 *         is loaded.
 *
 * @warning The returned pointer is owned internally and must not
 *          be freed or modified by the caller unless explicitly allowed.
 */
SongData *sound_get_current_song_data(void);

/**
 * @brief Shuts down the sound subsystem.
 *
 * Stops playback and decoding, releases audio device resources,
 * frees internal buffers, and performs any necessary cleanup.
 *
 * After calling this function, the sound system must be reinitialized
 * before further use.
 */
void sound_shutdown(void);

/**
 * @brief Immediately switches to the next track.
 *
 * Forces an immediate track change without performing any fade-out
 * or buffered transition.
 *
 * @note Typically used for user-initiated skip operations.
 */
void sound_switch_track_immediate(void);

/**
 * @brief Cleans up the internal audio ring buffer.
 *
 * Clears buffered audio data and resets the ring buffer state
 * to prevent stale or residual samples from being played.
 *
 * @note Should be called when stopping playback or resetting
 *       the decoding pipeline.
 */
void sound_ringbuffer_cleanup(void);

/**
 * @brief Enables or disables file switching.
 *
 * Controls whether the system is allowed to switch between
 * audio files (e.g., during track transitions).
 *
 * @param value true to allow file switching, false to prevent it.
 */
void set_switch_files(bool value);

/**
 * @brief Returns whether slot A is currently in use.
 *
 * Indicates which internal decoder slot is active. The system
 * typically maintains two slots (A and B) for seamless track
 * transitions.
 *
 * @return true if slot A is currently active; false if slot B is active.
 */
bool sound_is_using_slot_A(void);

/**
 * @brief Returns the bit depth of the format.
 *
 *
 * @return An int indicating the bit depth.
 */
int sound_get_bit_depth(ma_format format);

#endif
