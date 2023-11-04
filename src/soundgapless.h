#ifndef SOUNDGAPLESS_H
#define SOUNDGAPLESS_H
#include "../include/miniaudio/miniaudio.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdatomic.h>
#include <sys/wait.h>
#include "songloader.h"

#ifndef USERDATA_STRUCT
#define USERDATA_STRUCT
typedef struct
{
        char *filenameA;
        char *filenameB;
        SongData *songdataA;
        SongData *songdataB;
        SongData *currentSongData;
        ma_uint32 currentPCMFrame;
} UserData;
#endif

#ifndef PCMFILEDATASOURCE_STRUCT
#define PCMFILEDATASOURCE_STRUCT
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
        FILE *fileA;
        FILE *fileB;
        bool switchFiles;
        int currentFileIndex;
        bool seekRequested;
        float seekPercentage;
        ma_uint64 totalFrames;
} PCMFileDataSource;
#endif

#define CHANNELS 2
#define SAMPLE_RATE 192000
#define SAMPLE_WIDTH 3
#define SAMPLE_FORMAT ma_format_s24

extern ma_int32 *g_audioBuffer;
extern bool repeatEnabled;
extern bool shuffleEnabled;
extern double seekElapsed;

void createAudioDevice(UserData *userData);

void resumePlayback();

void pausePlayback();

void togglePausePlayback();

bool isPaused();

void cleanupPlaybackDevice();

void freeAudioBuffer();

bool isPlaybackDone();

void skip();

void seekPercentage(float percent);

#endif