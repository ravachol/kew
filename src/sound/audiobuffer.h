#ifndef AUDIOBUFFER_H
#define AUDIOBUFFER_H

#include "audiotypes.h"
#include <miniaudio.h>
#include <stdbool.h>

#ifndef MAX_BUFFER_SIZE
#define MAX_BUFFER_SIZE 32768
#endif

void *get_audio_buffer(void);
void reset_audio_buffer(void);
void freeAudioBuffer(void);
void set_audio_buffer(void *buf, int num_samples, ma_uint32 sample_rate, ma_uint32 channels, ma_format format);

void set_buffer_ready(bool val);
void set_buffer_size(int value);
int get_fft_size(void);
bool is_buffer_ready(void);

int32_t unpack_s24(const ma_uint8 *p);

#endif
