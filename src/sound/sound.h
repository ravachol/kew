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

UserData *getUserData(void);
bool isContextInitialized(void);
int createAudioDevice(void);
int switchAudioImplementation(void);
void cleanupAudioContext(void);
ma_result callReadPCMFrames(ma_data_source *pDataSource, ma_format format,
                            void *pFramesOut, ma_uint64 framesRead,
                            ma_uint32 channels, ma_uint64 remainingFrames,
                            ma_uint64 *pFramesToRead);
#endif
