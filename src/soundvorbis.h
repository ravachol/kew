#ifndef SOUNDVORBIS_H
#define SOUNDVORBIS_H
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "songloader.h"
#include "soundcommon.h"

MA_API ma_result ma_libvorbis_read_pcm_frames_wrapper(void *pDecoder, void* pFramesOut, long unsigned int frameCount, long unsigned int* pFramesRead);

MA_API ma_result ma_libvorbis_seek_to_pcm_frame_wrapper(void *pDecoder, long long int frameIndex, ma_seek_origin origin);

MA_API ma_result ma_libvorbis_get_cursor_in_pcm_frames_wrapper(void *pDecoder,long long int* pCursor);

void vorbis_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount);

#endif
