#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <math.h>
#include <dirent.h>
#include <stdbool.h>
#include "file.h"
#include "stringfunc.h"
#include "../include/miniaudio/miniaudio.h"
#define BUFFER_SIZE 1024

extern ma_int16 *g_audioBuffer;

int playSoundFile(const char *filePath);

void resumePlayback();

void pausePlayback();

bool isPaused();

void cleanupPlaybackDevice();

int adjustVolumePercent(int volumeChange);

int isPlaybackDone();

double getDuration(const char *filepath);

int adjustVolumePercent(int volumeChange);

void stopPlayback();