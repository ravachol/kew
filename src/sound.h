#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dirent.h>
#include <stdbool.h>
#include "file.h"
#include "stringextensions.h"
#include "../include/miniaudio/miniaudio.h"
#define BUFFER_SIZE 1024

extern ma_int16 *g_audioBuffer;

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);

int playSoundFileDefault(const char* filePath);

int convertAacToPcmFile(const char* filePath, const char* outputFilePath);

void pcm_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);

int playPcmFile(const char* filePath);

int playAacFile(const char* filePath);

int playSoundFile(const char* filePath);

int playPlaylist(char* filePath);

void resumePlayback();

void pausePlayback();

bool isPaused();

void cleanupPlaybackDevice();

int adjustVolumePercent(int volumeChange);

int isPlaybackDone();

double getDuration(const char* filepath);

int adjustVolumePercent(int volumeChange);