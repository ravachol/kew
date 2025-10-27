#include "audiobuffer.h"
#include <stdio.h>
#include <string.h>

static int fft_size = 2048;
static int hop_size = 512;
static int fft_size_milliseconds = 45;
static int write_head = 0;
static int buf_size;

static bool buffer_ready = false;

static float audio_buffer[MAX_BUFFER_SIZE];

int closest_power_of_two(int x)
{
        int n = 1;
        while (n < x)
                n <<= 1;
        return n;
}

bool is_buffer_ready(void) { return buffer_ready; }

void set_buffer_ready(bool val) { buffer_ready = val; }


int get_buffer_size(void) { return buf_size; }

void set_buffer_size(int value) { buf_size = value; }

// Sign-extend s24
ma_int32 unpack_s24(const ma_uint8 *p)
{
        ma_int32 sample = p[0] | (p[1] << 8) | (p[2] << 16);
        if (sample & 0x800000)
                sample |= ~0xFFFFFF;
        return sample;
}

void set_audio_buffer(void *buf, int numFrames, ma_uint32 sample_rate,
                    ma_uint32 channels, ma_format format)
{
        int bufIndex = 0;

        // Dynamically determine FFT and hop size
        float hopFraction = 0.25f; // 25% hop (75% overlap)

        // Compute power-of-two window/hop sizes in samples
        int wantFFTSamples = (int)(fft_size_milliseconds * sample_rate / 1000.0f);
        fft_size = closest_power_of_two(wantFFTSamples); // 2048 or 4096
        int wantHopSamples =
            (int)(fft_size * hopFraction);            // 25% of window length
        hop_size = closest_power_of_two(wantHopSamples); // 256, 512, 1024

        if (fft_size > MAX_BUFFER_SIZE)
                fft_size = MAX_BUFFER_SIZE;

        // Ensure hop is never >= window
        if (hop_size >= fft_size)
                hop_size = fft_size / 2; // fallback minimum overlap

        while (bufIndex < numFrames)
        {

                if (write_head >= fft_size)
                        break;

                int framesLeft = numFrames - bufIndex;
                int spaceLeft = fft_size - write_head;
                int framesToCopy =
                    framesLeft < spaceLeft ? framesLeft : spaceLeft;

                switch (format)
                {
                case ma_format_u8:
                {
                        ma_uint8 *src = (ma_uint8 *)buf + bufIndex * channels;
                        for (int i = 0; i < framesToCopy; ++i)
                        {
                                float sum = 0.0f;
                                for (ma_uint32 ch = 0; ch < channels; ++ch)
                                {
                                        // Convert 0..255 to -1..1
                                        sum += ((float)src[i * channels + ch] -
                                                128.0f) /
                                               128.0f;
                                }
                                audio_buffer[write_head++] = sum / channels;
                        }
                        break;
                }
                case ma_format_s16:
                {
                        ma_int16 *src = (ma_int16 *)buf + bufIndex * channels;
                        for (int i = 0; i < framesToCopy; ++i)
                        {
                                float sum = 0.0f;
                                for (ma_uint32 ch = 0; ch < channels; ++ch)
                                {
                                        sum += (float)src[i * channels + ch] /
                                               32768.0f;
                                }
                                audio_buffer[write_head++] = sum / channels;
                        }
                        break;
                }
                case ma_format_s24:
                {
                        ma_uint8 *src =
                            (ma_uint8 *)buf + bufIndex * channels * 3;
                        for (int i = 0; i < framesToCopy; ++i)
                        {
                                float sum = 0.0f;
                                for (ma_uint32 ch = 0; ch < channels; ++ch)
                                {
                                        int idx = i * channels * 3 + ch * 3;
                                        int32_t s = unpack_s24(&src[idx]);
                                        sum += (float)s / 8388608.0f;
                                }
                                audio_buffer[write_head++] = sum / channels;
                        }
                        break;
                }
                case ma_format_s32:
                {
                        int32_t *src = (int32_t *)buf + bufIndex * channels;
                        for (int i = 0; i < framesToCopy; ++i)
                        {
                                float sum = 0.0f;
                                for (ma_uint32 ch = 0; ch < channels; ++ch)
                                {
                                        sum += (float)src[i * channels + ch] /
                                               2147483648.0f;
                                }
                                audio_buffer[write_head++] = sum / channels;
                        }
                        break;
                }
                case ma_format_f32:
                {
                        float *src = (float *)buf + bufIndex * channels;
                        for (int i = 0; i < framesToCopy; ++i)
                        {
                                float sum = 0.0f;
                                for (ma_uint32 ch = 0; ch < channels; ++ch)
                                {
                                        sum += src[i * channels + ch];
                                }
                                audio_buffer[write_head++] = sum / channels;
                        }
                        break;
                }
                default:
                        fprintf(stderr,
                                "Unsupported format in set_audio_buffer!\n");
                        return;
                }
                bufIndex += framesToCopy;

                // Process full window(s), maintain overlap (hop)
                while (write_head >= fft_size)
                {
                        set_buffer_ready(true); // let main loop know FFT is ready

                        // Shift buffer for overlap (keep last fft_size-hop_size
                        // samples)
                        memmove(audio_buffer, audio_buffer + hop_size,
                                sizeof(float) * (fft_size - hop_size));
                        write_head -= hop_size;
                }
        }
}

void reset_audio_buffer(void)
{
        memset(audio_buffer, 0, sizeof(ma_int32) * MAX_BUFFER_SIZE);
        write_head = 0;
        set_buffer_ready(false);
}

void *get_audio_buffer(void) { return audio_buffer; }

int get_fft_size(void) { return fft_size; };
