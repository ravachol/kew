#ifndef SOUNDBUILTIN_H
#define SOUNDBUILTIN_H
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "songloader.h"
#include "soundcommon.h"

extern ma_data_source_vtable builtin_file_data_source_vtable;

void builtin_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount);

#endif
