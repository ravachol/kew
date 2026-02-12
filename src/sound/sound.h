/**
 * @file sound.[h]
 * @brief High-level audio playback interface.
 *
 * Provides a unified API for creating an audio device
 * and switching decoders.
 */

#ifndef SOUND_H
#define SOUND_H

#include "common/appstate.h"

#include <fcntl.h>
#include <miniaudio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

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


/**
 * @brief Reads PCM frames from a data source into the provided output buffer.
 *
 * This function reads a specified number of PCM frames from a given audio data source,
 * processing them according to the specified audio format (e.g., u8, s16, s24, s32, or f32).
 * It updates the remaining frames and the number of frames to read.
 *
 * @param p_data_source A pointer to the data source from which PCM frames are to be read.
 * @param format The audio format (e.g., ma_format_u8, ma_format_s16, etc.).
 * @param p_frames_out A pointer to the output buffer where the PCM frames will be stored.
 * @param frames_read The number of frames that have already been read.
 * @param channels The number of channels in the audio data (e.g., 2 for stereo).
 * @param remaining_frames The remaining frames to be read.
 * @param p_frames_to_read A pointer to the number of frames to read in the current operation.
 *
 * @return A result code indicating success or failure (e.g., MA_SUCCESS, MA_INVALID_ARGS).
 */
ma_result call_read_PCM_frames(ma_data_source *p_data_source, ma_format format,
                               void *p_frames_out, ma_uint64 frames_read,
                               ma_uint32 channels, ma_uint64 remaining_frames,
                               ma_uint64 *p_frames_to_read);
#endif
