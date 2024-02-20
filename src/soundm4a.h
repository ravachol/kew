#ifndef SOUNDM4A_H
#define SOUNDM4A_H
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "soundcommon.h"
#include "m4a.h"

void m4a_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount);

MA_API ma_result m4a_read_pcm_frames_wrapper(void *pDecoder, void* pFramesOut, size_t frameCount, size_t *pFramesRead);

MA_API ma_result m4a_seek_to_pcm_frame_wrapper(void *pDecoder, long long int frameIndex, ma_seek_origin origin);

MA_API ma_result m4a_get_cursor_in_pcm_frames_wrapper(void *pDecoder, long long int* pCursor);

#endif
