/**
 * @file sound_builtin.[h]
 * @brief Built-in audio backend implementation.
 *
 * Implements a simple internal audio output backend used when no
 * external library is available. Useful for portability and testing.
 */

#ifndef sound_builtin_H
#define sound_builtin_H

#include <miniaudio.h>

/**
 * @var builtin_file_data_source_vtable
 * @brief Vtable for the built-in audio data source.
 *
 * This vtable is used by the built-in audio backend to handle audio file
 * operations such as reading PCM frames.
 */
extern ma_data_source_vtable builtin_file_data_source_vtable;


/**
 * @brief Reads PCM frames from a built-in data source.
 *
 * This function is called to read a specified number of PCM frames from the
 * built-in audio data source. The function processes the audio frames and
 * places them into the provided output buffer.
 *
 * @param p_data_source Pointer to the data source object from which frames are read.
 * @param p_frames_out Pointer to the output buffer where the PCM frames will be stored.
 * @param frame_count The number of frames to read.
 * @param p_frames_read Pointer to a variable where the number of frames actually read will be stored.
 *
 * @note This function assumes the data source is correctly initialized and that
 * the provided buffer is large enough to hold the requested number of frames.
 */
void builtin_read_pcm_frames(ma_data_source *p_data_source, void *p_frames_out, ma_uint64 frame_count, ma_uint64 *p_frames_read);


/**
 * @brief Handles audio frames during the playback.
 *
 * This callback function is used during audio playback. It processes input and
 * output frames, applying any necessary transformations to the audio data.
 *
 * @param p_device Pointer to the audio device object.
 * @param p_frames_out Pointer to the buffer where the output audio frames will be stored.
 * @param p_frames_in Pointer to the buffer containing input audio frames.
 * @param frame_count The number of frames to process.
 *
 * @note This function is called repeatedly during audio playback and should be optimized for real-time performance.
 */
void builtin_on_audio_frames(ma_device *p_device, void *p_frames_out, const void *p_frames_in, ma_uint32 frame_count);

#endif
