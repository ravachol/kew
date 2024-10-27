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

#ifndef USERDATA_STRUCT
#define USERDATA_STRUCT
typedef struct
{
        SongData *songdataA;
        SongData *songdataB;
        bool songdataADeleted;
        bool songdataBDeleted;
        SongData *currentSongData;
        ma_uint32 currentPCMFrame;
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
        ma_uint32 currentPCMFrame;
        bool switchFiles;
        int currentFileIndex;
        ma_uint64 totalFrames;
        bool endOfListReached;
        bool restart;     
} AudioData;
#endif

extern UserData userData;

extern bool isContextInitialized;

int prepareNextDecoder(char *filepath);

int prepareNextOpusDecoder(char *filepath);

int prepareNextVorbisDecoder(char *filepath);

int prepareNextM4aDecoder(char *filepath);

void setDecoders(bool usingA, char *filePath);

int createAudioDevice(UserData *userData);

int switchAudioImplementation();

void cleanupAudioContext();

#endif
