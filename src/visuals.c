#include "visuals.h"

/*

visuals.c

 This file should contain only functions related to the spectrum visualizer.

*/

#ifndef MAX_BUFFER_SIZE
#define MAX_BUFFER_SIZE 4800
#endif

int bufferSize = 4800;
int prevBufferSize = 0;
float alpha = 0.1;
float lastMax = 90;
bool unicodeSupport = false;
fftwf_complex *fftInput = NULL;
fftwf_complex *fftOutput = NULL;

int bufferIndex = 0;

float magnitudeBuffer[MAX_BUFFER_SIZE] = {0.0f};
float lastMagnitudes[MAX_BUFFER_SIZE] = {0.0f};

int terminalSupportsUnicode()
{
        char *locale = setlocale(LC_ALL, "");
        if (locale != NULL)
                return 1;

        return 0;
}

void initVisuals()
{
        unicodeSupport = false;
  
        if (terminalSupportsUnicode() > 0)
                unicodeSupport = true;
}

void updateMagnitudes(int height, int width, float maxMagnitude, float *magnitudes)
{
        float exponent = 1.0;
        float decreaseFactor = 0.8;

        for (int i = 0; i < width; i++)
        {
                float normalizedMagnitude = magnitudes[i] / maxMagnitude;
                float scaledMagnitude = pow(normalizedMagnitude, exponent) * height;

                if (scaledMagnitude < lastMagnitudes[i])
                {
                        magnitudes[i] = lastMagnitudes[i] * decreaseFactor;
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

void calc(int height, int numBars, ma_int32 *audioBuffer, int bitDepth, fftwf_complex *fftInput, fftwf_complex *fftOutput, float *magnitudes, fftwf_plan plan)
{
        int bufferSize = getBufferSize();

        if (audioBuffer == NULL)
        {
                printf("Audio buffer is NULL.\n");
                return;
        }

        int j = 0;

        for (int i = 0; i < bufferSize; i++)
        {
                if (j >= bufferSize)
                {
                        printf("Exceeded FFT input buffer size.\n");
                        break;
                }

                ma_int32 sample = audioBuffer[i];
                float normalizedSample = 0;

                switch (bitDepth)
                {
                case 8:
                        normalizedSample = ((float)sample - 128) / 127.0f;
                        break;
                case 16:
                        normalizedSample = (float)sample / 32768.0f;
                        break;
                case 24:
                {
                        int lower24Bits = sample & 0xFFFFFF;
                        if (lower24Bits & 0x800000)
                        {
                                lower24Bits |= 0xFF000000; // Sign extension
                        }
                        normalizedSample = (float)lower24Bits / 8388607.0f;
                        break;
                }
                case 32: // Assuming 32-bit integers
                        normalizedSample = (float)sample / 2147483647.0f;
                        break;
                default:
                        printf("Unsupported bit depth: %d\n", bitDepth);
                        return;
                }

                fftInput[j][0] = normalizedSample;
                fftInput[j][1] = 0;
                j++;
        }

        for (int k = j; k < bufferSize; k++)
        {
                fftInput[k][0] = 0;
                fftInput[k][1] = 0;
        }

        // Apply Windowing (Hamming Window) only to populated samples
        for (int i = 0; i < j; i++)
        {
                float window = 0.54f - 0.46f * cos(2 * M_PI * i / (j - 1));
                fftInput[i][0] *= window;
        }

        fftwf_execute(plan); // Execute FFT

        clearMagnitudes(numBars, magnitudes);

        for (int i = 0, k = 0; i < numBars && k < j / 2; i += 2, k++)
        {
                // Compute magnitude for actual data points
                float magnitude = sqrtf(fftOutput[k][0] * fftOutput[k][0] + fftOutput[k][1] * fftOutput[k][1]);
                magnitudes[i] = magnitude; // Set actual magnitude

                if (i + 1 < numBars)
                {
                        float nextMagnitude;
                        if (k + 1 < j / 2)
                        {
                                // If not at the end, interpolate between this and the next actual magnitude
                                nextMagnitude = sqrtf(fftOutput[k + 1][0] * fftOutput[k + 1][0] + fftOutput[k + 1][1] * fftOutput[k + 1][1]);
                                magnitudes[i + 1] = (magnitude + nextMagnitude) / 2.0f; // Simple average for smoothing
                        }
                        else
                        {
                                // If at the end, could replicate the last magnitude or use a different logic for the filler
                                magnitudes[i + 1] = magnitude; // Replicating the last magnitude
                        }
                }
        }

        float maxMagnitude = calcMaxMagnitude(numBars, magnitudes);
        updateMagnitudes(height, numBars, maxMagnitude, magnitudes);
}

wchar_t *getUpwardMotionChar(int level)
{
        switch (level)
        {
        case 0:
                return L" ";
        case 1:
                return L"▁";
        case 2:
                return L"▂";
        case 3:
                return L"▃";
        case 4:
                return L"▄";
        case 5:
                return L"▅";
        case 6:
                return L"▆";
        case 7:
                return L"▇";
        default:
                return L"█";
        }
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

void printSpectrum(int height, int width, float *magnitudes, PixelData color, int indentation, bool useProfileColors)
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
                        if (!useProfileColors)
                        {
                                tmp = increaseLuminosity(color, round(j * height * 4));
                                printf("\033[38;2;%d;%d;%dm", tmp.r, tmp.g, tmp.b);
                        }
                }
                else
                {
                        setDefaultTextColor();
                }
                if (isPaused())
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
                                                if (unicodeSupport)
                                                {
                                                        printf(" %S", getUpwardMotionChar(10));
                                                }
                                                else
                                                {
                                                        printf(" █");
                                                }
                                        }
                                        else if (magnitudes[i] + 1 >= j && unicodeSupport)
                                        {
                                                int firstDecimalDigit = (int)(fmod(magnitudes[i] * 10, 10));
                                                printf(" %S", getUpwardMotionChar(firstDecimalDigit));
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

void freeVisuals()
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

void drawSpectrumVisualizer(int height, int width, PixelData c, int indentation, bool useProfileColors)
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

        printSpectrum(height, numBars, magnitudes, color, indentation, useProfileColors);

        fftwf_destroy_plan(plan);
}
