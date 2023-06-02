#include <stdbool.h>
#include "../include/miniaudio/miniaudio.h"

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

float getDuration(const char* filepath, const char* tempFile);