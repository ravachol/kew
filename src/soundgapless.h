#ifndef SOUNDGAPLESS_H
#define SOUNDGAPLESS_H
#include <miniaudio.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdatomic.h>
#include <pthread.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "file.h"
#include "songloader.h"
#include "soundcommon.h"
#include "soundbuiltin.h"
#include "soundopus.h"
#include "soundvorbis.h"
#include "soundm4a.h"
#include "volume.h"

#ifndef USERDATA_STRUCT
#define USERDATA_STRUCT
typedef struct
{
        char *filenameA;
        char *filenameB;
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
        const char *filenameA;
        const char *filenameB;
        ma_format format;
        ma_uint32 channels;
        ma_uint32 sampleRate;
        ma_uint32 currentPCMFrame;
        ma_decoder decoderA;
        ma_decoder decoderB;
        ma_decoder currentDecoder;
        FILE *fileA;
        FILE *fileB;
        bool switchFiles;
        int currentFileIndex;
        ma_uint64 totalFrames;
        bool endOfListReached;
        bool restart;     
} AudioData;
#endif

extern AudioData audioData;

extern UserData userData;

void setDecoders(bool usingA, char *filePath);

void createAudioDevice(UserData *userData);

int switchAudioImplementation();

void cleanupAudioContext();

void cleanupAudioData();

#endif
