#include "visuals.h"

/*

visuals.c

 This file should contain only functions related to the spectrum visualizer.

*/

#ifndef MAX_BUFFER_SIZE
#define MAX_BUFFER_SIZE 4800
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int bufferSize = 4800;
int prevBufferSize = 0;
float alpha = 0.2f;
float lastMax = -1.0f;
fftwf_complex *fftInput = NULL;
fftwf_complex *fftOutput = NULL;

int bufferIndex = 0;

float magnitudeBuffer[MAX_BUFFER_SIZE] = {0.0f};
float lastMagnitudes[MAX_BUFFER_SIZE] = {0.0f};
float smoothedMagnitudes[MAX_BUFFER_SIZE];
float maxPossibleMagnitude = 0;

#define MOVING_AVERAGE_WINDOW_SIZE 2

void updateMagnitudes(int height, int width, float maxMagnitude, float *magnitudes)
{
        if (maxMagnitude == 0.0f)
        {
                maxMagnitude = 1.0f;
        }
        
        // Apply moving average smoothing to magnitudes
        for (int i = 0; i < width; i++)
        {
                float sum = magnitudes[i];
                int count = 1;

                // Calculate moving average using a window centered at the current frequency bin
                for (int j = 1; j <= MOVING_AVERAGE_WINDOW_SIZE / 2; j++)
                {
                        if (i - j >= 0)
                        {
                                sum += magnitudes[i - j];
                                count++;
                        }
                        if (i + j < width)
                        {
                                sum += magnitudes[i + j];
                                count++;
                        }
                }

                // Compute the smoothed magnitude by averaging
                smoothedMagnitudes[i] = sum / count;
        }

        // Update magnitudes array with smoothed values
        memcpy(magnitudes, smoothedMagnitudes, width * sizeof(float));
        // Apply adaptive decay factor to the smoothed magnitudes
        float exponent = 1.0f;

        for (int i = 0; i < width; i++)
        {
                float normalizedMagnitude = magnitudes[i] / maxMagnitude;
                normalizedMagnitude = fminf(normalizedMagnitude, 1.0f);

                float scaledMagnitude = powf(normalizedMagnitude, exponent) * height;

                // Adaptive decay based on magnitude
                float decreaseFactor = 0.8f + 0.2f * (normalizedMagnitude);
                float decayedMagnitude = lastMagnitudes[i] * decreaseFactor;

                if (scaledMagnitude < decayedMagnitude)
                {
                        magnitudes[i] = decayedMagnitude;
                }
                else
                {
                        magnitudes[i] = scaledMagnitude;
                }
                lastMagnitudes[i] = magnitudes[i];
        }
}

float calcMaxMagnitude(int numBars, float *magnitudes)
{
        float maxMagnitude = 0.0f;
        for (int i = 0; i < numBars; i++)
        {
                if (magnitudes[i] > maxMagnitude)
                {
                        maxMagnitude = magnitudes[i];
                }
        }

        if (lastMax < 0.0f)
        {
                lastMax = maxMagnitude;
                return maxMagnitude;
        }

        lastMax = (1 - alpha) * lastMax + alpha * maxMagnitude; // Apply exponential smoothing
        return lastMax;
}

void clearMagnitudes(int width, float *magnitudes)
{
        for (int i = 0; i < width; i++)
        {
                magnitudes[i] = 0.0f;
        }
}

void enhancePeaks(int numBars, float *magnitudes, int height)
{
        for (int i = 1; i < numBars - 1; i++)
        {
                if (magnitudes[i] > magnitudes[i - 1] && magnitudes[i] > magnitudes[i + 1])
                {
                        magnitudes[i] *= 1.3f; // Emphasize the peak

                        magnitudes[i] = fminf(magnitudes[i], (float)height);
                }
        }
}

void calc(int height, int numBars, ma_int32 *audioBuffer, int bitDepth, fftwf_complex *fftInput, fftwf_complex *fftOutput, float *magnitudes, fftwf_plan plan)
{
        int bufferSize = getBufferSize();

        if (audioBuffer == NULL)
        {
                fprintf(stderr, "Audio buffer is NULL.\n");
                return;
        }

        for (int i = 0; i < bufferSize; i++)
        {
                ma_int32 sample = audioBuffer[i];
                float normalizedSample = 0.0f;

                switch (bitDepth)
                {
                case 8:
                        normalizedSample = ((float)sample - 128.0f) / 127.0f;
                        break;
                case 16:
                        normalizedSample = (float)sample / 32768.0f;
                        break;
                case 24:
                {
                        int32_t lower24Bits = sample & 0xFFFFFF;
                        if (lower24Bits & 0x800000)
                        {
                                lower24Bits |= 0xFF000000; // Sign extension
                        }
                        normalizedSample = (float)lower24Bits / 8388608.0f;
                        break;
                }
                case 32: // Assuming 32-bit integers
                        normalizedSample = (float)sample / 2147483648.0f;
                        break;
                default:
                        fprintf(stderr, "Unsupported bit depth: %d\n", bitDepth);
                        return;
                }

                fftInput[i][0] = normalizedSample;
                fftInput[i][1] = 0.0f;
        }

        // Apply Windowing (Hamming Window)
        if (bufferSize > 1)
        {
                for (int i = 0; i < bufferSize; i++)
                {
                        float window = 0.54f - 0.46f * cosf(2.0f * M_PI * i / (bufferSize - 1));
                        fftInput[i][0] *= window;
                }
        }

        fftwf_execute(plan); // Execute FFT

        clearMagnitudes(numBars, magnitudes);

        int halfSize = bufferSize / 2;
        int limit = (numBars < halfSize) ? numBars : halfSize;

        for (int i = 0; i < limit; i++)
        {
                // Compute magnitude for each frequency bin
                float real = fftOutput[i][0];
                float imag = fftOutput[i][1];
                float magnitude = sqrtf(real * real + imag * imag);
                magnitudes[i] = magnitude;
        }

        // Normalize and update magnitudes for visualization
        float maxMagnitude = calcMaxMagnitude(limit, magnitudes);

        updateMagnitudes(height, limit, maxMagnitude, magnitudes);

        enhancePeaks(numBars, magnitudes, height);
}

char *upwardMotionChars[] = {
    " ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};

char *getUpwardMotionChar(int level)
{
        if (level < 0 || level > 8)
        {
                level = 8;
        }
        return upwardMotionChars[level];
}

int calcSpectrum(int height, int numBars, fftwf_complex *fftInput, fftwf_complex *fftOutput, float *magnitudes, fftwf_plan plan)
{

        ma_int32 *g_audioBuffer = getAudioBuffer();

        ma_format format = getCurrentFormat();

        if (format == ma_format_unknown)
                return -1;

        int bitDepth = 32;

        switch (format)
        {
        case ma_format_u8:
                bitDepth = 8;
                break;

        case ma_format_s16:
                bitDepth = 16;
                break;

        case ma_format_s24:
                bitDepth = 24;
                break;

        case ma_format_f32:
        case ma_format_s32:
                bitDepth = 32;
                break;
        default:
                break;
        }

        calc(height, numBars, g_audioBuffer, bitDepth, fftInput, fftOutput, magnitudes, plan);

        return 0;
}

PixelData increaseLuminosity(PixelData pixel, int amount)
{
        PixelData pixel2;
        pixel2.r = pixel.r + amount <= 255 ? pixel.r + amount : 255;
        pixel2.g = pixel.g + amount <= 255 ? pixel.g + amount : 255;
        pixel2.b = pixel.b + amount <= 255 ? pixel.b + amount : 255;

        return pixel2;
}

void printSpectrum(int height, int width, float *magnitudes, PixelData color, int indentation, bool useConfigColors)
{
        printf("\n");
        clearRestOfScreen();

        PixelData tmp;

        for (int j = height; j > 0; j--)
        {
                printf("\r");
                printBlankSpaces(indentation);
                if (color.r != 0 || color.g != 0 || color.b != 0)
                {
                        if (!useConfigColors)
                        {
                                tmp = increaseLuminosity(color, round(j * height * 4));
                                printf("\033[38;2;%d;%d;%dm", tmp.r, tmp.g, tmp.b);
                        }
                }
                else
                {
                        setDefaultTextColor();
                }
                if (isPaused() || isStopped())
                {
                        for (int i = 0; i < width; i++)
                        {
                                printf("  ");
                        }
                }
                else
                {
                        for (int i = 0; i < width; i++)
                        {
                                if (j >= 0)
                                {
                                        if (magnitudes[i] >= j)
                                        {
                                                printf(" %s", getUpwardMotionChar(10));
                                        }
                                        else if (magnitudes[i] + 1 >= j)
                                        {
                                                int firstDecimalDigit = (int)(fmod(magnitudes[i] * 10, 10));
                                                printf(" %s", getUpwardMotionChar(firstDecimalDigit));
                                        }
                                        else
                                        {
                                                printf("  ");
                                        }
                                }
                        }
                }
                printf("\n ");
        }
        printf("\r");
        fflush(stdout);
}

void freeVisuals(void)
{
        if (fftInput != NULL)
        {
                fftwf_free(fftInput);
                fftInput = NULL;
        }
        if (fftOutput != NULL)
        {
                fftwf_free(fftOutput);
                fftOutput = NULL;
        }
}

void drawSpectrumVisualizer(int height, int width, PixelData c, int indentation, bool useConfigColors)
{
        bufferSize = getBufferSize();
        PixelData color;
        color.r = c.r;
        color.g = c.g;
        color.b = c.b;

        int numBars = (width / 2);
        height = height - 1;

        if (height <= 0 || width <= 0)
        {
                return;
        }

        if (bufferSize <= 0)
                return;

        if (bufferSize != prevBufferSize)
        {
                lastMax = -1.0f;

                freeVisuals();

                fftInput = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * bufferSize);
                if (fftInput == NULL)
                {
                        return;
                }

                fftOutput = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * bufferSize);
                if (fftOutput == NULL)
                {
                        fftwf_free(fftInput);
                        fftInput = NULL;
                        return;
                }
                prevBufferSize = bufferSize;
        }

        fftwf_plan plan = fftwf_plan_dft_1d(bufferSize, fftInput, fftOutput, FFTW_FORWARD, FFTW_ESTIMATE);

        float magnitudes[numBars];
        for (int i = 0; i < numBars; i++)
        {
                magnitudes[i] = 0.0f;
        }

        calcSpectrum(height, numBars, fftInput, fftOutput, magnitudes, plan);

        printSpectrum(height, numBars, magnitudes, color, indentation, useConfigColors);

        fftwf_destroy_plan(plan);
}
