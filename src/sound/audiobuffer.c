#include "audiobuffer.h"
#include <stdio.h>
#include <string.h>

static int fftSize = 2048;
static int hopSize = 512;
static int fftSizeMilliseconds = 45;
static int writeHead = 0;
static int bufSize;

static bool bufferReady = false;

static float audioBuffer[MAX_BUFFER_SIZE];

int closestPowerOfTwo(int x)
{
        int n = 1;
        while (n < x)
                n <<= 1;
        return n;
}

bool isBufferReady(void) { return bufferReady; }

void setBufferReady(bool val) { bufferReady = val; }


int getBufferSize(void) { return bufSize; }

void setBufferSize(int value) { bufSize = value; }

// Sign-extend s24
ma_int32 unpack_s24(const ma_uint8 *p)
{
        ma_int32 sample = p[0] | (p[1] << 8) | (p[2] << 16);
        if (sample & 0x800000)
                sample |= ~0xFFFFFF;
        return sample;
}

void setAudioBuffer(void *buf, int numFrames, ma_uint32 sampleRate,
                    ma_uint32 channels, ma_format format)
{
        int bufIndex = 0;

        // Dynamically determine FFT and hop size
        float hopFraction = 0.25f; // 25% hop (75% overlap)

        // Compute power-of-two window/hop sizes in samples
        int wantFFTSamples = (int)(fftSizeMilliseconds * sampleRate / 1000.0f);
        fftSize = closestPowerOfTwo(wantFFTSamples); // 2048 or 4096
        int wantHopSamples =
            (int)(fftSize * hopFraction);            // 25% of window length
        hopSize = closestPowerOfTwo(wantHopSamples); // 256, 512, 1024

        if (fftSize > MAX_BUFFER_SIZE)
                fftSize = MAX_BUFFER_SIZE;

        // Ensure hop is never >= window
        if (hopSize >= fftSize)
                hopSize = fftSize / 2; // fallback minimum overlap

        while (bufIndex < numFrames)
        {

                if (writeHead >= fftSize)
                        break;

                int framesLeft = numFrames - bufIndex;
                int spaceLeft = fftSize - writeHead;
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
                                audioBuffer[writeHead++] = sum / channels;
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
                                audioBuffer[writeHead++] = sum / channels;
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
                                audioBuffer[writeHead++] = sum / channels;
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
                                audioBuffer[writeHead++] = sum / channels;
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
                                audioBuffer[writeHead++] = sum / channels;
                        }
                        break;
                }
                default:
                        fprintf(stderr,
                                "Unsupported format in setAudioBuffer!\n");
                        return;
                }
                bufIndex += framesToCopy;

                // Process full window(s), maintain overlap (hop)
                while (writeHead >= fftSize)
                {
                        setBufferReady(true); // let main loop know FFT is ready

                        // Shift buffer for overlap (keep last fftSize-hopSize
                        // samples)
                        memmove(audioBuffer, audioBuffer + hopSize,
                                sizeof(float) * (fftSize - hopSize));
                        writeHead -= hopSize;
                }
        }
}

void resetAudioBuffer(void)
{
        memset(audioBuffer, 0, sizeof(ma_int32) * MAX_BUFFER_SIZE);
        writeHead = 0;
        setBufferReady(false);
}

void *getAudioBuffer(void) { return audioBuffer; }

int getFftSize(void) { return fftSize; };
