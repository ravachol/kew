#ifndef AUDIO_FILEINFO_H
#define AUDIO_FILEINFO_H

#include "common/common.h"

#include <miniaudio.h>

void get_file_info(const char *filename, ma_uint32 *sample_rate, ma_uint32 *channels, ma_format *format);
void get_m4a_file_info(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sample_rate, ma_channel *channelMap);
void get_m4a_extra_info(const char *filename, int *avgBitRate, k_m4adec_filetype *fileType);
void get_vorbis_file_info(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sample_rate, ma_channel *channelMap);
void get_opus_file_info(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sample_rate, ma_channel *channelMap);
void get_webm_file_info(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sample_rate, ma_channel *channelMap);

#endif
