/**
 * @file audiobuffer.h
 * @brief Related to the buffer used by the visualizer.
 *
 * Provides an api for stopping, starting and so on.
 * and switching decoders.
 */

#ifndef AUDIOBUFFER_H
#define AUDIOBUFFER_H

#include <miniaudio.h>

#include <stdbool.h>
#include <stdint.h>

#ifndef MAX_BUFFER_SIZE
#define MAX_BUFFER_SIZE 32768
#endif

/**
 * @brief Gets a pointer to the audio buffer.
 *
 * This function returns a pointer to the audio buffer used for storing
 * audio samples.
 *
 * @return A pointer to the audio buffer.
 */
void *get_audio_buffer(void);

/**
 * @brief Resets the audio buffer.
 *
 * This function clears the audio buffer and resets its state, including
 * resetting the write head and setting the buffer to not be ready.
 */
void reset_audio_buffer(void);

/**
 * @brief Frees the audio buffer.
 *
 * This function deallocates the memory used by the audio buffer and resets
 * the associated state.
 */
void freeAudioBuffer(void);

/**
 * @brief Sets the buffer ready flag.
 *
 * This function sets the buffer ready state, which is used to signal
 * whether the buffer is ready for processing.
 *
 * @param val The value to set the buffer ready state to (true or false).
 */
void set_buffer_ready(bool val);

/**
 * @brief Gets the FFT size.
 *
 * This function returns the current FFT size used for processing audio data.
 *
 * @return The current FFT size in samples.
 */
int get_fft_size(void);

/**
 * @brief Checks if the buffer is ready.
 *
 * This function returns the current state of the buffer's readiness, which
 * indicates whether the buffer is ready for processing.
 *
 * @return `true` if the buffer is ready, `false` otherwise.
 */
bool is_buffer_ready(void);

/**
 * @brief Unpacks a 24-bit signed sample from 3 bytes to a 32-bit signed integer.
 *
 * This function converts a 24-bit signed audio sample stored in 3 bytes into
 * a 32-bit signed integer by sign-extending the value.
 *
 * @param p Pointer to the 3-byte audio sample.
 *
 * @return The unpacked 32-bit signed sample.
 */
int32_t unpack_s24(const ma_uint8 *p);

/**
 * @brief Pushes audiodata to the ringbuffer used by spectrum visualizer
 *
 *
 * @param src Pointer to the audio frames.
 * @param frames The number of frames.
 * @param channels The number of channels.
 *
 */
void visualizer_ringbuffer_push(const float *src, ma_uint32 frames, ma_uint32 channels);

/**
 * @brief Prepares the audio buffer for the spectrum visualizer
 *
 * @param sample_rate The sample rate of the current audio.
 * @param channels The number of channels.
 *
 */
void prepare_visualizer_audiobuffer(ma_uint32 sample_rate, ma_uint32 channels);

#endif
