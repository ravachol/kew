#ifndef SOUNDGAPLESS_H
#define SOUNDGAPLESS_H
#include "../include/miniaudio/miniaudio.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include "songloader.h"

extern ma_int32 *g_audioBuffer;
extern bool skipping;

#ifndef PCMFILE_STRUCT
#define PCMFILE_STRUCT
typedef struct
{
    char *filename;
    FILE *file;
} PCMFile;
#endif

#ifndef USERDATA_STRUCT
#define USERDATA_STRUCT
typedef struct
{
    PCMFile pcmFileA;
    PCMFile pcmFileB;
    SongData* songdataA;
    SongData* songdataB;
    ma_uint32 currentFileIndex;
    ma_uint32 currentPCMFrame;
    int endOfListReached;
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
} PCMFileDataSource;
#endif

extern UserData userData;
extern bool repeatEnabled;
extern bool shuffleEnabled;
extern SongData *currentSongData;

void createAudioDevice(UserData *userData);

void resumePlayback();

void pausePlayback();

void togglePausePlayback();

bool isPaused();

int convertToPcmFile(const char *filePath, const char *outputFilePath);

void cleanupPlaybackDevice();

int adjustVolumePercent(int volumeChange);

bool isPlaybackDone();

double getDuration(const char *filepath);

int adjustVolumePercent(int volumeChange);

void setVolume(int volume);

void stopPlayback();

void skip();

#endif