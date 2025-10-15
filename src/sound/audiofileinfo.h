#ifndef AUDIO_FILEINFO_H
#define AUDIO_FILEINFO_H

#include "common/common.h"

#include <miniaudio.h>

void getFileInfo(const char *filename, ma_uint32 *sampleRate, ma_uint32 *channels, ma_format *format);
void getM4aFileInfo(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sampleRate, ma_channel *channelMap);
void getM4aExtraInfo(const char *filename, int *avgBitRate, k_m4adec_filetype *fileType);
void getVorbisFileInfo(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sampleRate, ma_channel *channelMap);
void getOpusFileInfo(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sampleRate, ma_channel *channelMap);
void getWebmFileInfo(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sampleRate, ma_channel *channelMap);

#endif
