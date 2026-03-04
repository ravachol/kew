/**
 * @file playback_system.h
 * @brief Low-level audio system integration.
 *
 * Manages initialization, configuration, and teardown of
 * audio backends and decoders. Provides the connection between
 * playback operations and sound output subsystems.
 */

/**
 * @brief Create the sound system.
 *
 * Initializes the underlying audio backend and prepares the playback
 * device for audio output.
 *
 * @return 0 on success, or a negative value if device creation fails.
 */
#include <stdbool.h>
int create_sound_system(void);

/**
 * @brief Shutdown the sound system.
 *
 * Releases resources associated with the audio playback device.
 * This version does not acquire any synchronization locks and
 * must only be called when it is safe to do so.
 */
void shutdown_sound_system(void);

/**
 * @brief Uninitializes the audio output device and resets playback state.
 *
 * Shuts down the underlying sound system device and updates the global
 * playback state to reflect that no next song is currently loaded and
 * that the system is waiting for the next track.
 *
 * Specifically:
 * - Calls sound_system_uninit_device() to release audio hardware resources.
 * - Sets PlaybackState::loadedNextSong to false.
 * - Sets PlaybackState::waitingForNext to true.
 *
 * @note After calling this function, audio playback cannot resume until
 *       the device is reinitialized.
 *
 * @warning This function should not be called while audio callbacks or
 *          playback threads are actively using the device.
 */
void uninit_device(void);

/**
 * @brief Skip to the next track or audio implementation.
 *
 * Resets relevant playback state, clears repeat flags, and triggers
 * switching to the next buffered audio stream. If playback is stopped,
 * forces an implementation switch immediately. Optionally triggers a UI refresh.
 */
void skip(void);

/**
 * @brief Switch the active audio implementation.
 *
 * Requests the playback backend to switch between audio buffers or
 * decoder implementations (e.g., in a double-buffered setup).
 */
void switch_decoder(void);

/**
 * @brief Ensure that the default theme pack is available.
 *
 * Verifies the presence of the application's default themes and
 * installs or restores them if necessary.
 */
void ensure_default_theme_pack(void);

/**
 * @brief Returns whether audio should start playing.
 *
 * Retrieves the internal flag that indicates whether the audio
 * device or playback pipeline should be started.
 *
 *
 * @return Non-zero if audio start is requested, 0 otherwise.
 *
 * @note This flag is typically set via start_playing().
 */
int should_start_playing(void);

/**
 * @brief Signals that the audio should be started.
 *
 * @param value indicating if sound should start playing.
 */
void start_playing(bool value);
