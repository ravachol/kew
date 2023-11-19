#ifndef SOUNDPCM_H
#define SOUNDPCM_H
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "soundcommon.h"

extern ma_data_source_vtable pcm_file_data_source_vtable;

void pcm_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount);

#endif
