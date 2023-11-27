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
#include "soundpcm.h"

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

void setDecoders(bool usingA, char *filePath);

void createAudioDevice(UserData *userData);

void switchAudioImplementation();

void cleanupAudioContext();

#endif
