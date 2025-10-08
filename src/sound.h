#ifndef SOUND_H
#define SOUND_H

#include <fcntl.h>
#include <miniaudio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "file.h"
#include "songloader.h"
#include "soundbuiltin.h"
#include "soundcommon.h"
#include "common.h"

#ifndef USERDATA_STRUCT
#define USERDATA_STRUCT

typedef struct
{
        SongData *songdataA;
        SongData *songdataB;
        bool songdataADeleted;
        bool songdataBDeleted;
        SongData *currentSongData;
        ma_uint64 currentPCMFrame;
} UserData;
#endif

#ifndef AUDIODATA_STRUCT
#define AUDIODATA_STRUCT
typedef struct
{
        ma_data_source_base base;
        UserData *pUserData;
        ma_format format;
        ma_uint32 channels;
        ma_uint32 sampleRate;
        ma_uint64 currentPCMFrame;
        ma_uint32 avgBitRate;
        bool switchFiles;
        int currentFileIndex;
        ma_uint64 totalFrames;
        bool endOfListReached;
        bool restart;
} AudioData;
#endif

extern UserData userData;

extern bool isContextInitialized;

int createAudioDevice();

int switchAudioImplementation(void);

void resumePlayback(void);

void cleanupAudioContext(void);

float get_current_playback_time(void);

#endif
