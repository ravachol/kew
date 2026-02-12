#ifndef AUDIOBUFFER_H
#define AUDIOBUFFER_H

#include "audiotypes.h"
#include <miniaudio.h>
#include <stdbool.h>

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
 * @brief Sets the audio buffer with the given data.
 *
 * This function initializes the audio buffer with the specified parameters,
 * including the number of samples, sample rate, channels, and format. It
 * dynamically calculates FFT and hop sizes based on the sample rate.
 *
 * @param buf Pointer to the buffer that contains audio data.
 * @param num_samples The number of audio samples in the buffer.
 * @param sample_rate The sample rate of the audio data.
 * @param channels The number of channels in the audio data.
 * @param format The format of the audio data (e.g., `ma_format_u8`, `ma_format_s16`).
 */
void set_audio_buffer(void *buf, int num_samples, ma_uint32 sample_rate, ma_uint32 channels, ma_format format);


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
 * @brief Sets the buffer size.
 *
 * This function sets the size of the buffer used for audio processing.
 *
 * @param value The size of the buffer in samples.
 */
void set_buffer_size(int value);


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

#endif
