/**
 * @file sound.[h]
 * @brief High-level audio playback interface.
 *
 * Provides a unified API for creating an audio device
 * and switching decoders.
 */

#ifndef SOUND_H
#define SOUND_H

#include "common/appstate.h"

#include <fcntl.h>
#include <miniaudio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

bool is_context_initialized(void);
int create_audio_device(void);
int switch_audio_implementation(void);
void cleanup_audio_context(void);
ma_result call_read_PCM_frames(ma_data_source *pDataSource, ma_format format,
                            void *pFramesOut, ma_uint64 framesRead,
                            ma_uint32 channels, ma_uint64 remainingFrames,
                            ma_uint64 *pFramesToRead);
#endif
