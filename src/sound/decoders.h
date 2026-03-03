/**
 * @file decoders.h
 * @brief Decoders.
 *
 * Decoder related functions.
 */

#ifndef DECODERS_H
#define DECODERS_H

#include "audiotypes.h"
#include "sound/sound_facade.h"
#include "webm.h"

#include <miniaudio.h>
#include <miniaudio_libopus.h>
#include <miniaudio_libvorbis.h>
#include <stdbool.h>

typedef void (*uninit_func)(void *decoder);
typedef int (*init_func)(const char *filepath, ma_decoding_backend_config *config, void *decoder);
typedef void (*setup_func)(void *decoder, void *firstDecoder);
typedef void (*ma_data_callback_proc)(
    ma_device *,
    void *,
    const void *,
    ma_uint32);

typedef void *(*decoder_getter_func)(void);

typedef ma_result (*decoder_format_func)(
    ma_data_source *data_source,
    ma_format *format,
    ma_uint32 *channels,
    ma_uint32 *sample_rate,
    ma_channel *channel_map,
    size_t channel_map_cap);

typedef ma_result (*decoder_seek_func)(
    void *decoder,
    long long frameIndex,
    ma_seek_origin origin);

typedef int (*get_cursor_func)(
    ma_data_source *p_data_source,
    long long *p_cursor);

typedef int (*create_audio_device_func)(
    void *user_data,
    sound_system_t *sound,
    ma_device *device,
    ma_context *context,
    decoder_getter_func get_decoder,
    ma_data_callback_proc callback);

typedef struct
{
        /* Returns the “current decoder” or first decoder */
        decoder_getter_func getDecoder;

        /* Reads basic file info (format, channels, sample rate, channel map) */
        void (*get_file_info)(
            const char *file_path,
            ma_format *p_format,
            ma_uint32 *p_channels,
            ma_uint32 *p_sample_rate,
            ma_channel *p_channel_map);

        /* Retrieves the format of an existing decoder */
        decoder_format_func get_decoder_format;

        /* Optional per-decoder seek function */
        decoder_seek_func seek_to_pcm_frame;

        /* Optional cursor getter function */
        get_cursor_func get_cursor;

        /* PCM read callback */
        ma_data_callback_proc callback;

        /* Function to create an audio device for this decoder */
        create_audio_device_func create_audio_device;

        /* Which implementation this is (VORBIS, OPUS, WEBM, BUILTIN, etc.) */
        enum decoder_type implType;

        /* Supports gapless playback? (WebM is false) */
        bool supportsGapless;

        /* Setup function (assign onRead/onSeek/onTell, user data, etc.) */
        setup_func setup_decoder;

        /* Decoder lifecycle management */
        size_t decoderSize; /* sizeof(decoder struct) */
        init_func init;
        uninit_func uninit;

        /* Arrays and indices for chaining multiple decoders of this type */
        void **decoderArray; /* array of decoder pointers */
        void **firstDecoder; /* pointer to first decoder pointer */
        int *decoderIndex;   /* pointer to current index */

} CodecOps;

const CodecOps *get_codec_ops(enum decoder_type type);

typedef struct
{
        char *extension;
        CodecOps ops;
} CodecEntry;

/**
 * @var builtin_file_data_source_vtable
 * @brief Vtable for the built-in audio data source.
 *
 * This vtable is used by the built-in audio backend to handle audio file
 * operations such as reading PCM frames.
 */
extern ma_data_source_vtable builtin_file_data_source_vtable;

/**
 * @brief Switches between two specific decoders.
 *
 * This function toggles the given decoder index between 0 and 1. It is used to
 * switch between different decoder configurations in a decoder chain.
 *
 * @param decoder_index A pointer to the decoder index to toggle.
 */
void switch_specific_decoder(int *decoder_index);

/**
 * @brief Switches between multiple decoders in the system.
 *
 * This function toggles the decoder index for all available decoders in the system.
 * It is used to switch between different decoder types (e.g., Opus, Vorbis, WebM, etc.)
 *
 * @see switch_specific_decoder
 */
void switch_decoder_index(void);

/**
 * @brief Retrieves codec operations for a given implementation type.
 *
 * Returns the CodecOps structure describing the function pointers
 * and metadata associated with the specified audio implementation.
 *
 * @param type  Audio implementation identifier.
 *
 * @return Pointer to the corresponding CodecOps structure,
 *         or NULL if the implementation is not supported.
 */
const CodecOps *get_codec_ops(enum decoder_type type);

/**
 * @brief Determines the appropriate codec implementation for a file.
 *
 * Inspects the file path (typically by extension) and returns the
 * matching CodecOps entry for decoding.
 *
 * @param file_path  Path to the audio file.
 *
 * @return Pointer to the corresponding CodecOps structure,
 *         or NULL if no suitable codec is found.
 */
const CodecOps *find_codec_ops(const char *file_path);

/**
 * @brief Uninitializes the previously used decoder.
 *
 * This function uninitializes the decoder at the opposite index of the current decoder
 * and releases any associated resources.
 *
 * @param decoder_array An array of decoders.
 * @param index The index of the current decoder.
 * @param uninit The uninitialization function to call for the decoder type.
 */
void uninit_previous_decoder(void **decoder_array, int index, uninit_func uninit);

/**
 * @brief Checks if the file uses a decoder that comes with miniaudio.
 *
 * This function checks whether the given file can be decoded using a built-in decoder,
 * such as for WAV, FLAC, or MP3 files.
 *
 * @param file_path The file path to check for a suitable decoder.
 * @return True if a built-in decoder is available for the file, false otherwise.
 */
bool is_decoder_native(const char *file_path);

/**
 * @brief Clears the decoder chain.
 *
 * This function clears the current decoder chain, effectively disconnecting the
 * current decoder from the data source.
 */
void clear_decoder_chain(void);

/**
 * @brief Gets the first available decoder.
 *
 * This function returns the first decoder in the system.
 *
 * @return A pointer to the first available decoder.
 */
void *get_first_decoder(void);

/**
 * @brief Gets the current decoder.
 *
 * @return A pointer to the current decoder.
 */
void *get_current_decoder(void);

/**
 * @brief Is the decoder capable of seeking.
 *
 * @return an int indicating 0 for seeking disabled and 1 for seeking allowed.
 */
int can_decoder_seek(void *decoder);

/**
 * @brief Gets the current decoder AudioImplementation.
 *
 * @return An AudioImplementation.
 */
enum decoder_type get_current_decoder_implementation_type(void);

/**
 * @brief Resets the decoders in the system.
 *
 * This function resets all decoders in the system, releasing resources and
 * uninitializing them as necessary.
 *
 */
void reset_decoders();

/**
 * @brief Sets the current decoder AudioImplementation
 *
 * @param new_implType The type of audio implementation.
 */
void set_current_decoder_implType(enum decoder_type new_implType);

/**
 * @brief Sets the next decoder in the decoder chain.
 *
 * This function adds the provided decoder to the chain, uninitializing the previous
 * decoder as necessary. It is used to set the next decoder for playback.
 *
 * @param decoder A pointer to the next decoder to set.
 * @param new_implType The type of audio implementation.
 */
void set_next_decoder(void *decoder, const enum decoder_type new_implType);

/**
 * @brief Prepares the next decoder for any supported format.
 *
 * This function prepares the next decoder for a general file, checking if the format
 * is compatible and initializing the decoder accordingly.
 *
 * @param filepath The file path to prepare.
 * @return 0 on success, -1 on failure.
 */
int prepare_next_decoder(const char *filepath, SongData *song, const CodecOps *ops);

#endif
