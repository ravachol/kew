#include "visuals.h"

/*

visuals.c

 This file should contain only functions related to the spectrum visualizer.

*/

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_BARS 64

int bufferSize = 8192;
int prevBufferSize = 0;
float alpha = 0.2f;
float lastMax = -1.0f;
float *fftInput = NULL;
fftwf_complex *fftOutput = NULL;

int bufferIndex = 0;

ma_format format = ma_format_unknown;
ma_uint32 sampleRate = 44100;

float magnitudeBuffer[MAX_BARS] = {0.0f};
float lastMagnitudes[MAX_BARS] = {0.0f};
float smoothedMagnitudes[MAX_BARS] = {0.0f};
float minMaxMagnitude = 100.0f;
float enhancePeak = 1.4f;
float exponent = 0.7f; // Lower than 1.0 makes quiet sounds more visible
float baseDecay = 0.8f;
float rangeDecay = 0.2f;
float baseAttack = 0.2f;
float rangeAttack = 0.6f;
float maxMagnitude = 0.0f;

// Snare detection
float snarePeak[MAX_BARS] = {0.0f};
int spikeTimeout = 150;
static int framesSinceSpike = 0;
bool snareDetected = false;
float snarePeakDecay = 0.8f;

#define MOVING_AVERAGE_WINDOW_SIZE 2

void smoothMovingAverageMagnitudes(int numBars, float *magnitudes)
{
        // Apply moving average smoothing to magnitudes
        for (int i = 0; i < numBars; i++)
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
                        if (i + j < numBars)
                        {
                                sum += magnitudes[i + j];
                                count++;
                        }
                }

                // Compute the smoothed magnitude by averaging
                smoothedMagnitudes[i] = sum / count;
        }

        // Update magnitudes array with smoothed values
        memcpy(magnitudes, smoothedMagnitudes, numBars * sizeof(float));
}

float applyAttackandDecay(float linearVal, float lastMagnitude)
{
        float ratio = linearVal / maxMagnitude; // 0..1
        if (ratio > 1.0f)
                ratio = 1.0f; // Clamp

        float decreaseFactor = baseDecay + rangeDecay * ratio;
        float decayedMagnitude = linearVal * decreaseFactor;

        float attackFactor = baseAttack + rangeAttack * ratio;

        float newVal;

        if (linearVal < decayedMagnitude)
        {
                // Decay
                newVal = decayedMagnitude;
        }
        else
        {
                // Attack
                newVal = lastMagnitude + attackFactor * (linearVal - lastMagnitude);
        }

        return newVal;
}

void updateMagnitudes(int height, int numBars, float maxMagnitude, float *magnitudes)
{
        smoothMovingAverageMagnitudes(numBars, magnitudes);

        for (int i = 0; i < numBars; i++)
        {
                float newVal = applyAttackandDecay(magnitudes[i], lastMagnitudes[i]);

                lastMagnitudes[i] = newVal;

                // Normalize
                float displayRatio = newVal / maxMagnitude;
                if (displayRatio > 1.0f)
                        displayRatio = 1.0f;
                magnitudes[i] = powf(displayRatio, exponent) * height;
        }
}

float calcMaxMagnitude(int numBars, float *magnitudes)
{
        maxMagnitude = 0.0f;
        for (int i = 0; i < numBars; i++)
        {
                if (magnitudes[i] > maxMagnitude)
                {
                        maxMagnitude = magnitudes[i];
                }
        }

        if (maxMagnitude < minMaxMagnitude)
                maxMagnitude = minMaxMagnitude;

        if (lastMax < 0.0f)
        {
                lastMax = maxMagnitude;
                return maxMagnitude;
        }

        // Apply exponential smoothing
        lastMax = (1 - alpha) * lastMax + alpha * maxMagnitude;

        return lastMax;
}

void clearMagnitudes(int numBars, float *magnitudes)
{
        for (int i = 0; i < numBars; i++)
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
                        magnitudes[i] *= enhancePeak;

                        magnitudes[i] = fminf(magnitudes[i], (float)height);
                }
        }
}

void applyBlackmanHarris(float *fftInput, int bufferSize)
{
        for (int i = 0; i < bufferSize; i++)
        {
                float alpha0 = 0.35875f;
                float alpha1 = 0.48829f;
                float alpha2 = 0.14128f;
                float alpha3 = 0.01168f;

                float fraction = (float)i / (float)(bufferSize - 1); // i / (N-1)
                float window =
                    alpha0 - alpha1 * cosf(2.0f * M_PI * fraction) + alpha2 * cosf(4.0f * M_PI * fraction) - alpha3 * cosf(6.0f * M_PI * fraction);

                fftInput[i] *= window;
        }
}

float getAvgAmplitude(const float *samples, int windowSize)
{
        float sum = 0.0f;
        for (int i = 0; i < windowSize; i++)
        {
                sum += fabsf(samples[i]);
        }
        return sum / windowSize;
}

bool detectSpike(const float *magnitudes, int windowSize, ma_uint32 sampleRate)
{
        static float amplitudeAvg = 0.0f;
        static const float alpha = 0.85f;      // Baseline smoothing
        static const float spikeFactor = 2.0f; // "Twice" baseline = spike

        float amplitude = getAvgAmplitude(magnitudes, windowSize);

        // Update rolling average (baseline)
        amplitudeAvg = alpha * amplitudeAvg + (1.0f - alpha) * amplitude;

        float timeout = spikeTimeout * (((float)windowSize / (float)sampleRate) + 0.01f); // Roughly every spikeTimeout ms

        bool spikeDetected = (amplitude > spikeFactor * amplitudeAvg);

        if (spikeDetected)
        {
                framesSinceSpike = 0;
        }
        else
        {
                framesSinceSpike++;
                if (framesSinceSpike > timeout)
                {
                        amplitudeAvg = 0.0f;
                }
        }
        return spikeDetected;
}

void detectSnare(const float *fftInput, int numBins, int sampleRate, int fftSize)
{
        if (detectSpike(fftInput, numBins, sampleRate))
        {
                // Convert frequencies to bin indices for ~150..4000 Hz
                int binStart = (int)(150.0f * (float)fftSize / (float)sampleRate);
                int binEnd = (int)(4000.0f * (float)fftSize / (float)sampleRate);

                // Clamp them to [0..(numBins-1)]
                if (binStart < 0)
                        binStart = 0;
                if (binEnd >= numBins)
                        binEnd = numBins - 1;

                if (binStart == 0)
                        binStart = 1; // First bar reserved for deeper bases

                // If the range is invalid, no snare can be detected.
                if (binStart > binEnd)
                {
                        snareDetected = false;
                        return;
                }

                // Sum midrange bins vs total energy
                float sumMidBins = 0.0f;
                float sumAllBins = 0.0f;
                for (int b = 1; b < numBins; b++)
                {
                        sumAllBins += fftInput[b];
                        if (b >= binStart && b <= binEnd)
                        {
                                sumMidBins += fftInput[b];
                        }
                }

                // Avoid divide-by-zero
                if (sumAllBins < 1e-9f)
                {
                        snareDetected = false;
                        return;
                }

                // Fraction of energy in the snare band
                float midrangeEnergy = sumMidBins / sumAllBins;

                // Threshold for calling it a snare
                if (midrangeEnergy > 0.12f)
                        snareDetected = true;
                else
                        snareDetected = false;
        }
        else
        {
                snareDetected = false;
        }
}

void elevateSnare(float *magnitudes, int numBars, int bufferSize, int height, ma_uint32 sampleRate)
{
        if (snareDetected)
        {
                snareDetected = false;

                int binStart = (int)(150.0f * bufferSize / (float)sampleRate);
                int binEnd = (int)(4000.0f * bufferSize / (float)sampleRate);

                if (binStart < 0)
                        binStart = 0;
                if (binEnd >= numBars)
                        binEnd = numBars - 1;

                if (binEnd < binStart)
                {
                        goto DecayPeakAndCombine;
                }

                // Find the strongest bar (index)
                float maxVal = 0.0f;
                int snareBin = binStart;
                for (int b = binStart; b <= binEnd; b++)
                {
                        if (magnitudes[b] > maxVal)
                        {
                                maxVal = magnitudes[b];
                                snareBin = b;
                        }
                }

                int barIndex = snareBin;

                if (barIndex >= numBars)
                {
                        barIndex = numBars - 1;
                }

                float spikeFactor = 2.0f;
                float spikedValue = maxVal * spikeFactor;
                if (spikedValue > height)
                {
                        spikedValue = (float)height - (height*0.1);
                }

                if (spikedValue > snarePeak[barIndex] && barIndex > 0)
                {
                        snarePeak[barIndex] = spikedValue;
                }
        }
DecayPeakAndCombine:

        for (int i = 1; i < numBars; i++)
        {
                float displayVal = fmaxf(magnitudes[i], snarePeak[i]);
                snarePeak[i] *= snarePeakDecay;
                magnitudes[i] = displayVal;
        }
}

void calc(int height, int numBars, ma_int32 *audioBuffer, int bitDepth, float *fftInput, fftwf_complex *fftOutput, float *magnitudes, fftwf_plan plan)
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

                fftInput[i] = normalizedSample;
        }

        int halfSize = bufferSize / 2;
        int limit = (numBars < halfSize) ? numBars : halfSize;

        applyBlackmanHarris(fftInput, bufferSize);

        fftwf_execute(plan);

        clearMagnitudes(numBars, magnitudes);

        for (int i = 0; i < limit; i++)
        {
                float real = fftOutput[i][0];
                float imag = fftOutput[i][1];
                float magnitude = sqrtf(real * real + imag * imag);
                magnitudes[i] = magnitude;
        }

        float maxMagnitude = calcMaxMagnitude(limit, magnitudes);

        updateMagnitudes(height, limit, maxMagnitude, magnitudes);

        enhancePeaks(numBars, magnitudes, height);

        float fftMagnitudesHalf[halfSize];

        for (int i = 0; i < halfSize; i++)
        {
                float real = fftOutput[i][0];
                float imag = fftOutput[i][1];
                float magnitude = sqrtf(real * real + imag * imag);
                fftMagnitudesHalf[i] = magnitude;
        }

        detectSnare(fftMagnitudesHalf, halfSize, sampleRate, bufferSize);

        elevateSnare(magnitudes, numBars, bufferSize, height, sampleRate);
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

int calcSpectrum(int height, int numBars, float *fftInput, fftwf_complex *fftOutput, float *magnitudes, fftwf_plan plan)
{

        ma_int32 *g_audioBuffer = getAudioBuffer();

        getCurrentFormatAndSampleRate(&format, &sampleRate);

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

void printSpectrum(int height, int numBars, float *magnitudes, PixelData color, int indentation, bool useConfigColors, int visualizerColorType)
{
        printf("\n");

        PixelData tmp;

        for (int j = height; j > 0; j--)
        {
                printf("\r");
                printBlankSpaces(indentation);
                if (color.r != 0 || color.g != 0 || color.b != 0)
                {
                        if (!useConfigColors && (visualizerColorType == 0 || visualizerColorType == 2))
                        {
                                if (visualizerColorType == 0)
                                {
                                        tmp = increaseLuminosity(color, round(j * height * 8));
                                }
                                else if (visualizerColorType == 2)
                                {
                                        tmp = increaseLuminosity(color, round((height - j) * height * 8));
                                }
                                printf("\033[38;2;%d;%d;%dm", tmp.r, tmp.g, tmp.b);
                        }
                }
                else
                {
                        setDefaultTextColor();
                }

                if (isPaused() || isStopped())
                {
                        for (int i = 0; i < numBars; i++)
                        {
                                printf("  ");
                        }
                        printf("\n ");
                        continue;
                }

                for (int i = 0; i < numBars; i++)
                {
                        if (!useConfigColors && visualizerColorType == 1)
                        {
                                tmp = (PixelData){color.r / 2, color.g / 2, color.b / 2}; // Make colors half as bright before increasing brightness
                                tmp = increaseLuminosity(tmp, round(magnitudes[i] * 10 * 4));

                                printf("\033[38;2;%d;%d;%dm", tmp.r, tmp.g, tmp.b);
                        }

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
                printf("\n ");
        }
        printf("\r");
        fflush(stdout);
}

void freeVisuals(void)
{
        if (fftInput != NULL)
        {
                free(fftInput);
                fftInput = NULL;
        }
        if (fftOutput != NULL)
        {
                fftwf_free(fftOutput);
                fftOutput = NULL;
        }
}

void drawSpectrumVisualizer(int height, int numBars, PixelData c, int indentation, bool useConfigColors, int visualizerColorType)
{
        bufferSize = getBufferSize();
        PixelData color;
        color.r = c.r;
        color.g = c.g;
        color.b = c.b;

        numBars = (numBars / 2);
        height = height - 1;

        if (height <= 0 || numBars <= 0)
        {
                return;
        }

        if (numBars > MAX_BARS)
                numBars = MAX_BARS;

        if (bufferSize <= 0)
        {
                for (int i = 0; i <= height; i++)
                {
                        printf("\n");
                }
                return;
        }
        if (bufferSize != prevBufferSize)
        {
                lastMax = -1.0f;

                freeVisuals();

                fftInput = (float *)malloc(sizeof(float) * bufferSize);
                if (fftInput == NULL)
                {
                        for (int i = 0; i <= height; i++)
                        {
                                printf("\n");
                        }
                        return;
                }

                fftOutput = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * bufferSize);
                if (fftOutput == NULL)
                {
                        fftwf_free(fftInput);
                        fftInput = NULL;
                        for (int i = 0; i <= height; i++)
                        {
                                printf("\n");
                        }
                        return;
                }
                prevBufferSize = bufferSize;
        }

        fftwf_plan plan = fftwf_plan_dft_r2c_1d(bufferSize,
                                                fftInput,
                                                fftOutput,
                                                FFTW_ESTIMATE);

        float magnitudes[numBars];
        for (int i = 0; i < numBars; i++)
        {
                magnitudes[i] = 0.0f;
        }

        calcSpectrum(height, numBars, fftInput, fftOutput, magnitudes, plan);

        printSpectrum(height, numBars, magnitudes, color, indentation, useConfigColors, visualizerColorType);

        fftwf_destroy_plan(plan);
}
