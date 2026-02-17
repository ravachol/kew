/**
 * @file playback_system.h
 * @brief Low-level audio system integration.
 *
 * Manages initialization, configuration, and teardown of
 * audio backends and decoders. Provides the connection between
 * playback operations and sound output subsystems.
 */

#include "common/appstate.h"

#include "data/theme.h"

/**
 * @brief Create and initialize the audio playback device.
 *
 * Initializes the underlying audio backend and prepares the playback
 * device for audio output.
 *
 * @return 0 on success, or a negative value if device creation fails.
 */
int create_playback_device(void);

/**
 * @brief Safely clean up the playback device.
 *
 * Performs playback device cleanup while holding the application's
 * data source mutex to ensure thread safety.
 */
void playback_safe_cleanup(void);

/**
 * @brief Clean up the playback device.
 *
 * Releases resources associated with the audio playback device.
 * This version does not acquire any synchronization locks and
 * must only be called when it is safe to do so.
 */
void playback_cleanup(void);

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
void switch_audio_implementation(void);

/**
 * @brief Shut down the audio subsystem.
 *
 * Performs final cleanup of the playback backend and releases
 * audio-related resources prior to application termination.
 */
void sound_shutdown(void);

/**
 * @brief Unload song data from both playback buffers.
 *
 * Marks both SongData buffers as deleted and frees their associated
 * resources if they have not already been unloaded.
 *
 * @param user_data Pointer to the UserData structure containing
 *                  song buffer state.
 */
void unload_songs(UserData *user_data);

/**
 * @brief Free all allocated decoder instances.
 *
 * Resets and releases all audio decoders currently managed by
 * the playback system.
 */
void free_decoders(void);

/**
 * @brief Ensure that the default theme pack is available.
 *
 * Verifies the presence of the application's default themes and
 * installs or restores them if necessary.
 */
void ensure_default_theme_pack(void);

