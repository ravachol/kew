#ifndef SOUNDGAPLESS_H
#define SOUNDGAPLESS_H
#include "../include/miniaudio/miniaudio.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>

extern ma_int16 *g_audioBuffer;

#ifndef PCMFILE_STRUCT
#define PCMFILE_STRUCT
typedef struct {
    const char* filename;
    char* pcmData;
    ma_uint64 pcmDataSize;
} PCMFile;
#endif

#ifndef USERDATA_STRUCT
#define USERDATA_STRUCT
typedef struct {
    PCMFile pcmFileA;
    PCMFile pcmFileB;
    ma_uint32 currentFileIndex;
    ma_uint32 currentPCMFrame;
    int endOfListReached;
} UserData;
#endif

#ifndef PCMFILEDATASOURCE_STRUCT
#define PCMFILEDATASOURCE_STRUCT
typedef struct {
    ma_data_source_base base;
    UserData* pUserData;
    const char* filename;
    char* pcmData;
    ma_uint64 pcmDataSize;
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    ma_uint32 currentPCMFrame;
} PCMFileDataSource;
#endif

extern bool repeatEnabled;

void createAudioDevice(UserData *userData);

void resumePlayback();

void pausePlayback();

bool isPaused();

int convertToPcmFile(const char *filePath, const char *outputFilePath);

void cleanupPlaybackDevice();

int adjustVolumePercent(int volumeChange);

int isPlaybackDone();

double getDuration(const char *filepath);

int adjustVolumePercent(int volumeChange);

void stopPlayback();

void loadPcmFile(PCMFile* pcmFile, const char* filename);

void skip();

#endif