#ifndef AUDIOBUFFER_H
#define AUDIOBUFFER_H

#include "audiotypes.h"
#include <miniaudio.h>
#include <stdbool.h>

#ifndef MAX_BUFFER_SIZE
#define MAX_BUFFER_SIZE 32768
#endif

void *getAudioBuffer(void);
void resetAudioBuffer(void);
void freeAudioBuffer(void);
void setAudioBuffer(void *buf, int numSamples, ma_uint32 sampleRate, ma_uint32 channels, ma_format format);

void setBufferReady(bool val);
void setBufferSize(int value);
int getFftSize(void);
bool isBufferReady(void);

int32_t unpack_s24(const ma_uint8 *p);

#endif
