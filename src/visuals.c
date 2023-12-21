#include "visuals.h"
#include "albumart.h"
#include "complex.h"
#define CHANNELS 2
#define BEAT_THRESHOLD 0.3

#define MAGNITUDE_FLOOR_FRACTION 0.4
#ifndef MAX_BUFFER_SIZE
#define MAX_BUFFER_SIZE 4800
#endif

int bufferSize = 4800;
int prevBufferSize = 0;
float magnitudeCeil = 120;
float alpha = 0.1;
float lastMax = 120;
bool unicodeSupport = false;
fftwf_complex *fftInput = NULL;
fftwf_complex *fftOutput = NULL;
/*

visuals.c

 This file should contain only functions related to the spectrum visualizer.

*/
int bufferIndex = 0;

float magnitudeBuffer[MAX_BUFFER_SIZE] = {0.0f};
float lastMagnitudes[MAX_BUFFER_SIZE] = {0.0f};

void initVisuals()
{
        unicodeSupport = false;
        char *locale = setlocale(LC_ALL, "");
        if (locale != NULL)
                unicodeSupport = true;
}

void printBlankSpaces(int numSpaces)
{
        for (int i = 0; i < numSpaces; i++)
        {
                printf(" ");
        }
}

void updateMagnitudeBuffer(float magnitude)
{
        magnitudeBuffer[bufferIndex] = magnitude;
        bufferIndex = (bufferIndex + 1) % bufferSize;
}

float calculateMovingAverage()
{
        float sum = 0.0f;
        int numSamples = 0;
        for (int i = 0; i < bufferSize; i++)
        {
                sum += magnitudeBuffer[i];
                if (magnitudeBuffer[i] > 0.0f)
                {
                        numSamples++;
                }
        }
        numSamples = (numSamples > 0) ? numSamples : 1;
        return sum / numSamples;
}

float calculateThreshold()
{
        float sum = 0.0f;
        int numSamples = 0;
        for (int i = 0; i < bufferSize; i++)
        {
                sum += magnitudeBuffer[i];
                if (magnitudeBuffer[i] > 0.0f)
                {
                        numSamples++;
                }
        }
        numSamples = (numSamples > 0) ? numSamples : 1;
        float mean = sum / numSamples;

        float variance = 0.0f;
        for (int i = 0; i < bufferSize; i++)
        {
                float diff = magnitudeBuffer[i] - mean;
                variance += diff * diff;
        }
        variance /= numSamples;

        float stdDeviation = sqrtf(variance);
        float threshold = mean + (stdDeviation * BEAT_THRESHOLD);

        return threshold;
}

int detectBeats(float *magnitudes, int numBars)
{
        float avgMagnitude = 0.0f;
        int range = 3;
        range = (range > numBars) ? numBars : range;

        for (int i = 0; i < range; i++)
        {
                float magnitude = magnitudes[i];
                avgMagnitude += magnitude;
        }
        avgMagnitude /= range;
        updateMagnitudeBuffer(avgMagnitude);
        float threshold = calculateThreshold();

        for (int i = 0; i < range; i++)
        {
                if (magnitudes[i] > threshold)
                {
                        return 1;
                }
        }
        return 0;
}

void updateMagnitudes(int height, int width, float maxMagnitude, float *magnitudes)
{
        float exponent = 1.0;
        float jumpFactor = 0.0;
        float decreaseFactor = 0.7;
        int jumpAmount = ceil(height * 0.125);

        int beat = detectBeats(magnitudes, width);
        if (beat > 0)
        {
                jumpFactor = jumpAmount;
        }

        for (int i = 0; i < width; i++)
        {
                float normalizedMagnitude = magnitudes[i] / maxMagnitude;
                float scaledMagnitude = pow(normalizedMagnitude, exponent) * height + jumpFactor;

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
                return;
        }

        int j = 0;

        for (int i = 0; i < bufferSize; i++)
        {
                ma_int32 sample = audioBuffer[i];

                float normalizedSample;

                // Adjust normalization based on bit depth
                if (bitDepth == 8)
                {
                        normalizedSample = ((float)sample - 128) / 127.0f;
                }
                else if (bitDepth == 16)
                {
                        normalizedSample = (float)sample / 32768.0f;
                }
                else if (bitDepth == 24)
                {
                        // Extract the lower 24 bits from the sample
                        int lower24Bits = sample & 0xFFFFFF;

                        // Check if the 24th bit is set (indicating a negative value)
                        if (lower24Bits & 0x800000)
                        {
                                // Sign extension for two's complement
                                lower24Bits |= 0xFF000000;
                        }
                        else
                        {
                                // Ensure that the upper bits are cleared for positive values
                                lower24Bits &= 0x00FFFFFF;
                        }

                        // Normalize the 24-bit sample to the range [-1, 1]
                        normalizedSample = (float)lower24Bits / 8388607.0f;
                }
                else if (bitDepth == 32 || bitDepth == -32) // Assuming bitDepth == -32 for f32
                {
                        normalizedSample = (float)sample / 2147483647.0f;
                }
                else
                {
                        // Unsupported bit depth
                        return;
                }

                if (bitDepth == 32)
                {
                        if (i % 3 != 0)
                        {
                                continue;
                        }
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

        // Apply Windowing (Hamming Window)
        for (int i = 0; i < bufferSize; i++)
        {
                float window = 0.54f - 0.46f * cos(2 * M_PI * i / (bufferSize - 1));
                fftInput[i][0] *= window;
        }

        fftwf_execute(plan);
        clearMagnitudes(numBars, magnitudes);

        for (int i = 0; i < numBars; i++)
        {
                float magnitude = sqrtf(fftOutput[i][0] * fftOutput[i][0] + fftOutput[i][1] * fftOutput[i][1]);
                magnitudes[i] += magnitude;
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

void calcSpectrum(int height, int numBars, fftwf_complex *fftInput, fftwf_complex *fftOutput, float *magnitudes, fftwf_plan plan)
{

        ma_int32 *g_audioBuffer = getAudioBuffer();
        ma_decoder *decoder = getCurrentDecoder();
        int bitDepth = 24;

        ma_format format = SAMPLE_FORMAT;

        if (getCurrentImplementationType() == BUILTIN && decoder != NULL)
        {
                format = decoder->outputFormat;

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
        }

        calc(height, numBars, g_audioBuffer, bitDepth, fftInput, fftOutput, magnitudes, plan);
}

PixelData increaseLuminosity(PixelData pixel, int amount)
{
        PixelData pixel2;
        pixel2.r = pixel.r + amount <= 255 ? pixel.r + amount : 255;
        pixel2.g = pixel.g + amount <= 255 ? pixel.g + amount : 255;
        pixel2.b = pixel.b + amount <= 255 ? pixel.b + amount : 255;

        return pixel2;
}

PixelData decreaseLuminosity(PixelData pixel, int amount)
{
        PixelData pixel2;
        pixel2.r = pixel.r - amount >= amount ? pixel.r - amount : amount;
        pixel2.g = pixel.g - amount >= amount ? pixel.g - amount : amount;
        pixel2.b = pixel.b - amount >= amount ? pixel.b - amount : amount;

        return pixel2;
}

void printSpectrum(int height, int width, float *magnitudes, PixelData color)
{
        printf("\n");
        clearRestOfScreen();

        PixelData tmp;

        for (int j = height; j > 0; j--)
        {
                printf("\r");
                printBlankSpaces(indent);
                if (color.r != 0 || color.g != 0 || color.b != 0)
                {
                        if (!useProfileColors)
                        {
                                tmp = increaseLuminosity(color, round(j * height * 2));
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

void drawSpectrumVisualizer(int height, int width, PixelData c)
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
        printSpectrum(height, numBars, magnitudes, color);

        fftwf_destroy_plan(plan);
}
