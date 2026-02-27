/**
 * @file sound.h
 * @brief High-level audio playback interface.
 *
 * Provides a unified API for creating an audio device
 * and switching decoders.
 */

#ifndef SOUND_H
#define SOUND_H

#include <stdbool.h>


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
 * The function also attempts to switch to a new audio implementation.
 *
 * @return 0 on success, or -1 if an error occurs while switching the audio implementation.
 */
int pb_create_audio_device(void);

/**
 * @brief Switches the audio implementation for playback.
 *
 * This function handles the process of switching to a different audio implementation,
 * including loading the necessary song data and handling codec operations. It checks
 * whether the end of the audio list has been reached and attempts to switch to the appropriate codec.
 *
 * @return 0 if successful, or -1 if there is an error in switching the implementation.
 */
int pb_switch_audio_implementation(void);

/**
 * @brief Cleans up and uninitializes the audio context.
 *
 * This function releases the audio context and sets the initialization flag to false,
 * effectively cleaning up the resources associated with the audio context.
 */
void cleanup_audio_context(void);

#endif
