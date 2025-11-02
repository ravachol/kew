#ifndef AUDIO_FILEINFO_H
#define AUDIO_FILEINFO_H

#include "common/common.h"

#include <miniaudio.h>

void get_file_info(const char *filename, ma_uint32 *sample_rate, ma_uint32 *channels, ma_format *format);
void get_m4a_file_info(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sample_rate, ma_channel *channel_map);
void get_m4a_file_info_full(const char *filename, ma_format *format,
                            ma_uint32 *channels, ma_uint32 *sample_rate,
                            ma_channel *channel_map, int *avg_bit_rate,
                            k_m4adec_filetype *file_type);
void get_vorbis_file_info(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sample_rate, ma_channel *channel_map);
void get_opus_file_info(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sample_rate, ma_channel *channel_map);
void get_webm_file_info(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sample_rate, ma_channel *channel_map);

#endif
