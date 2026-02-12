/**
 * @file sound.[h]
 * @brief Decoders.
 *
 * Get audio file info.
 */

#ifndef AUDIO_FILEINFO_H
#define AUDIO_FILEINFO_H

#include "common/common.h"

#include <miniaudio.h>

/**
 * @brief Gets basic information about an audio file.
 *
 * This function retrieves the sample rate, number of channels, and audio
 * format of the specified audio file. The function uses a decoder to
 * extract this information.
 *
 * @param filename The path to the audio file.
 * @param sample_rate Pointer to a variable where the sample rate will be stored.
 * @param channels Pointer to a variable where the number of channels will be stored.
 * @param format Pointer to a variable where the audio format will be stored.
 */
void get_file_info(const char *filename, ma_uint32 *sample_rate, ma_uint32 *channels, ma_format *format);


/**
 * @brief Gets detailed information about a Vorbis encoded audio file.
 *
 * This function retrieves the format, number of channels, sample rate, and
 * channel mapping for an audio file encoded with Vorbis.
 *
 * @param filename The path to the Vorbis file.
 * @param format Pointer to a variable where the audio format will be stored.
 * @param channels Pointer to a variable where the number of channels will be stored.
 * @param sample_rate Pointer to a variable where the sample rate will be stored.
 * @param channel_map Pointer to a variable where the channel map will be stored.
 */
void get_vorbis_file_info(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sample_rate, ma_channel *channel_map);


/**
 * @brief Gets detailed information about an Opus encoded audio file.
 *
 * This function retrieves the format, number of channels, sample rate, and
 * channel mapping for an audio file encoded with Opus.
 *
 * @param filename The path to the Opus file.
 * @param format Pointer to a variable where the audio format will be stored.
 * @param channels Pointer to a variable where the number of channels will be stored.
 * @param sample_rate Pointer to a variable where the sample rate will be stored.
 * @param channel_map Pointer to a variable where the channel map will be stored.
 */
void get_opus_file_info(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sample_rate, ma_channel *channel_map);


/**
 * @brief Gets detailed information about a WebM audio file.
 *
 * This function retrieves the sample rate, number of channels, and audio
 * format of an audio file encoded in the WebM format. It ignores the
 * channel mapping.
 *
 * @param filename The path to the WebM file.
 * @param format Pointer to a variable where the audio format will be stored.
 * @param channels Pointer to a variable where the number of channels will be stored.
 * @param sample_rate Pointer to a variable where the sample rate will be stored.
 * @param channel_map Pointer to a variable where the channel map will be stored (not used).
 */
void get_webm_file_info(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sample_rate, ma_channel *channel_map);


/**
 * @brief Gets detailed information about an M4A audio file (Full Information).
 *
 * This function retrieves detailed information about an M4A audio file,
 * including format, number of channels, sample rate, channel mapping,
 * average bit rate, and file type.
 *
 * @param filename The path to the M4A file.
 * @param format Pointer to a variable where the audio format will be stored.
 * @param channels Pointer to a variable where the number of channels will be stored.
 * @param sample_rate Pointer to a variable where the sample rate will be stored.
 * @param channel_map Pointer to a variable where the channel map will be stored.
 * @param avg_bit_rate Pointer to a variable where the average bit rate will be stored (in kbps).
 * @param file_type Pointer to a variable where the file type will be stored.
 */
void get_m4a_file_info_full(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sample_rate, ma_channel *channel_map, int *avg_bit_rate, k_m4adec_filetype *file_type);


/**
 * @brief Gets basic M4A file information (Sample Rate, Channels, and Format).
 *
 * This function retrieves basic audio information (sample rate, channels, and
 * format) for an M4A file. It delegates the work to `get_m4a_file_info_full`,
 * but discards unused information such as the bit rate and file type.
 *
 * @param filename The path to the M4A file.
 * @param format Pointer to a variable where the audio format will be stored.
 * @param channels Pointer to a variable where the number of channels will be stored.
 * @param sample_rate Pointer to a variable where the sample rate will be stored.
 * @param channel_map Pointer to a variable where the channel map will be stored.
 */
void get_m4a_file_info(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sample_rate, ma_channel *channel_map);

#endif
