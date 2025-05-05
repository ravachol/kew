#include "visuals.h"

/*

visuals.c

 This file should contain only functions related to the spectrum visualizer.

*/

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_BARS 64

int prevBufferSize = 0;
float alpha = 0.02f;
float smoothFramesAlpha = 0.8f;
float lastMax = -1.0f;
float *fftInput = NULL;
float *fftPreviousInput = NULL;
fftwf_complex *fftOutput = NULL;

int bufferIndex = 0;

ma_format format = ma_format_unknown;
ma_uint32 sampleRate = 0;

float magnitudeBuffer[MAX_BARS] = {0.0f};
float lastMagnitudes[MAX_BARS] = {0.0f};
float smoothedMagnitudes[MAX_BARS] = {0.0f};
float smoothedFramesMagnitudes[MAX_BARS] = {0.0f};
float minMaxMagnitude = 10.0f;
float spacingPower = 1.0f; // 0.8 = Bars skewed toward bass values. 1.0 = Linear

float maxMagnitude = 50.0f;

float baseDecay = 0.72f;
float rangeDecay = 0.04f;
float baseAttack = 0.7f;
float rangeAttack = 0.6f;

float tweenFactor = 0.4f;
float tweenFactorFall = 0.2f;

float snareTweenFactor = 0.4f;
float snareTweenFactorFall = 0.2f;

float riseThresholdPercent = 0.03f;
float fallThresholdPercent = 0.03f;

bool snareDetected = false;
bool elevateBarsOnSnare = false;

bool fatBars = false;

#define MOVING_AVERAGE_WINDOW_SIZE 2

#define MIN_THRESHOLD 0.01f // adjust for silence sensitivity

void smoothMovingAverageMagnitudes(int numBars, float *magnitudes)
{
        // Apply moving average smoothing
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

                smoothedMagnitudes[i] = sum / count;
        }

        memcpy(magnitudes, smoothedMagnitudes, numBars * sizeof(float));
}

float applyAttackandDecay(float linearVal, float lastMagnitude, float maxMagnitude)
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
    for (int i = 0; i < numBars; i++)
    {
        float value = applyAttackandDecay(magnitudes[i], lastMagnitudes[i], maxMagnitude);

        float ratio = value / maxMagnitude;

        if (ratio < 0) ratio = 0.0f;
        if (ratio > 1) ratio = 1.0f;

        magnitudes[i] = ratio * height;
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

void enhancePeaks(int numBars, float *magnitudes, int height, float enhancePeak)
{
        for (int i = 2; i < numBars - 1; i++) // Don't enhance bass sounds as they already dominate too much in most music
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
        float alpha0 = 0.35875f;
        float alpha1 = 0.48829f;
        float alpha2 = 0.14128f;
        float alpha3 = 0.01168f;

        for (int i = 0; i < bufferSize; i++)
        {
                float fraction = (float)i / (float)(bufferSize - 1); // i / (N-1)
                float window =
                    alpha0 - alpha1 * cosf(2.0f * M_PI * fraction) + alpha2 * cosf(4.0f * M_PI * fraction) - alpha3 * cosf(6.0f * M_PI * fraction);

                fftInput[i] *= window;
        }
}

void smoothFrames(
    float *magnitudes,
    float *smoothedFramesMagnitudes,
    int numBars,
    float tweenFactor,
    float tweenFactorFall,
    float riseThresholdPercent,
    float fallThresholdPercent)
{
        for (int i = 0; i < numBars; i++)
        {
                float currentVal = smoothedFramesMagnitudes[i];
                float targetVal = magnitudes[i];
                float delta = targetVal - currentVal;

                float rise = tweenFactor;
                float fall = tweenFactorFall;

                float riseRef = fmax(currentVal, targetVal);
                float riseThreshold = riseThresholdPercent * riseRef;

                float fallRef = fmin(currentVal, targetVal);
                float fallThreshold = fallThresholdPercent * fallRef;

                if (delta > riseThreshold)
                {
                         smoothedFramesMagnitudes[i] += delta * rise;
                }
                else if (delta < -fallThreshold)
                {
                         smoothedFramesMagnitudes[i] += delta * fall;
                }
        }
}

// Average All Bins in Bar Range
void fillSpectrumBars(
    const fftwf_complex *fftOutput,
    int bufferSize,
    float sampleRate,
    float *magnitudes,
    int numBars,
    float minFreq,
    float maxFreq,
    float spacingPower)
{
        float *barFreqLo = (float *)malloc(sizeof(float) * numBars);
        float *barFreqHi = (float *)malloc(sizeof(float) * numBars);

        float logMin = log10f(minFreq);
        float logMax = log10f(maxFreq);

        int numBins = bufferSize / 2 + 1;
        float binSpacing = sampleRate / bufferSize;

        for (int i = 0; i < numBars; i++)
        {
                float tLo = (float)i / numBars;
                float tHi = (float)(i + 1) / numBars;

                float skewLo = powf(tLo, spacingPower);
                float skewHi = powf(tHi, spacingPower);

                barFreqLo[i] = powf(10.0f, logMin + (logMax - logMin) * skewLo);
                barFreqHi[i] = powf(10.0f, logMin + (logMax - logMin) * skewHi);
        }

        for (int i = 0; i < numBars; i++)
        {
                // Find bins covered by this bar range
                int binLo = (int)ceilf(barFreqLo[i] / binSpacing);
                int binHi = (int)floorf(barFreqHi[i] / binSpacing);

                // Clamp to valid bins
                if (binLo < 0)
                        binLo = 0;
                if (binHi >= numBins)
                        binHi = numBins - 1;

                // Special case: if range selects no bins, pick the closest one
                if (binHi < binLo)
                        binHi = binLo;

                float sum = 0.0f;
                int count = 0;
                for (int k = binLo; k <= binHi; k++)
                {
                        float real = fftOutput[k][0];
                        float imag = fftOutput[k][1];
                        sum += sqrtf(real * real + imag * imag);
                        count++;
                }
                magnitudes[i] = (count > 0) ? sum / count : 0.0f;
        }

        free(barFreqLo);
        free(barFreqHi);
}

void findBarIndicesForFreqRange(
    float minFreq, float maxFreq,
    int numBars, float overallMinFreq, float overallMaxFreq,
    int *outStart, int *outEnd, float spacingPower)
{
        int startBar = -1, endBar = -1;
        float logMin = log10f(overallMinFreq);
        float logMax = log10f(overallMaxFreq);

        for (int i = 0; i < numBars; ++i)
        {
                float tLo = (float)i / numBars;
                float tHi = (float)(i + 1) / numBars;

                float skewLo = powf(tLo, spacingPower);
                float skewHi = powf(tHi, spacingPower);

                float barFreqLo = powf(10.0f, logMin + (logMax - logMin) * skewLo);
                float barFreqHi = powf(10.0f, logMin + (logMax - logMin) * skewHi);

                // If this bar at least partly overlaps target band, include it
                if (barFreqHi >= minFreq && barFreqLo <= maxFreq)
                {
                        if (startBar < 0)
                                startBar = i;
                        endBar = i;
                }
        }
        if (startBar < 0)
                startBar = 0;
        if (endBar < 0)
                endBar = numBars - 1;
        *outStart = startBar;
        *outEnd = endBar;
}

float avgEnergyInBand(int startFreq, int endFreq, float *magnitudes,
                      int numBars, float minFreq, float maxFreq)
{
        int startBar, endBar;
        findBarIndicesForFreqRange(
            (float)startFreq, (float)endFreq,
            numBars, minFreq, maxFreq,
            &startBar, &endBar, spacingPower);

        float sum = 0.0f;
        int n = 0;
        for (int i = startBar; i <= endBar && i < numBars; i++)
        {
                sum += magnitudes[i];
                n++;
        }
        if (n <= 0)
                return 0.0f;
        return sum / n;
}

bool detectSnare(float *magnitudes, int numBars, float minFreq, float maxFreq)
{
        float snareBody = avgEnergyInBand(150, 900, magnitudes, numBars, minFreq, maxFreq);
        float snareAttack = avgEnergyInBand(1500, 4500, magnitudes, numBars, minFreq, maxFreq);
        float snareEnergy = 0.5 * snareBody + 1.0 * snareAttack; // Heavily bias toward attack

        static float runningSnareAvg = 0.0f;
        float alpha = 0.05f; // Smoothing. Lower = slower adapt (>percussion, <vocals)
        runningSnareAvg = (1.0f - alpha) * runningSnareAvg + alpha * snareEnergy;
        float threshold = 1.2f * runningSnareAvg;

        bool snareDetected = false;
        if (snareEnergy > threshold)
                snareDetected = true;

        return snareDetected;
}

void calc(
    int height,
    int numBars,
    ma_int32 *audioBuffer,
    int bitDepth,
    float *fftInput,
    fftwf_complex *fftOutput,
    float *magnitudes,
    fftwf_plan plan)
{
        if (!bufferReady)
                return;
        if (!audioBuffer)
        {
                fprintf(stderr, "Audio buffer is NULL.\n");
                return;
        }

        bufferReady = false;

        for (int i = 0; i < FFT_SIZE; i++)
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
                                lower24Bits |= 0xFF000000; // Sign extend
                        normalizedSample = (float)lower24Bits / 8388608.0f;
                        break;
                }
                case 32:
                        normalizedSample = (float)sample / 2147483648.0f;
                        break;
                default:
                        fprintf(stderr, "Unsupported bit depth: %d\n", bitDepth);
                        return;
                }
                fftInput[i] = normalizedSample;
        }

        applyBlackmanHarris(fftInput, FFT_SIZE);
        fftwf_execute(plan);

        clearMagnitudes(numBars, magnitudes);
        sampleRate = 44100;

        float minFreq = 20.0f;
        float maxFreq = (float)((sampleRate / 2 > 48000) ? 48000 : sampleRate / 2);

        fillSpectrumBars(
            fftOutput, FFT_SIZE, sampleRate, magnitudes, numBars,
            minFreq, maxFreq, spacingPower);

        snareDetected = detectSnare(magnitudes, numBars, minFreq, maxFreq);

        float maxMagnitude = calcMaxMagnitude(numBars, magnitudes);

        int snareStart, snareEnd;
        findBarIndicesForFreqRange(
            300.0f, 3000.0f, // Snare-like freq range is 150 - 3000
            numBars,
            minFreq, maxFreq,
            &snareStart, &snareEnd, spacingPower);

        updateMagnitudes(height, numBars, maxMagnitude, magnitudes);

        if (snareDetected && elevateBarsOnSnare)
        {
            for (int i = 0; i < numBars; i++)
            {
                float magnitude = magnitudes[i];

                float normalized = magnitude / maxMagnitude;

                // The lower the magnitude, the higher the boost factor
                float boostFactor = 1.0f + (2.5f * (1.0f - normalized));

                magnitudes[i] *= boostFactor;
            }
        }

        smoothFrames(magnitudes, smoothedFramesMagnitudes, numBars, tweenFactor, tweenFactorFall,
                     riseThresholdPercent, fallThresholdPercent);
}

char *upwardMotionCharsBlock[] = {
    " ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};

char *upwardMotionCharsBraille[] = {
    " ", "⣀", "⣀", "⣤", "⣤", "⣶", "⣶", "⣿", "⣿"};

char *inbetweenCharsRising[] = {
    " ", "⣠", "⣠", "⣴", "⣴", "⣾", "⣾", "⣿", "⣿"};

char *inbetweenCharsFalling[] = {
    " ", "⡀", "⡀", "⣄", "⣄", "⣦", "⣦", "⣷", "⣷"};

char *getUpwardMotionChar(int level, bool braille)
{
        if (level < 0 || level > 8)
        {
                level = 8;
        }
        if (braille)
                return upwardMotionCharsBraille[level];
        else
                return upwardMotionCharsBlock[level];
}

char *getInbetweendMotionChar(float magnitudePrev, float magnitudeNext, int prev, int next)
{
        if (prev < 0)
                prev = 0;
        if (prev > 8)
                prev = 8;
        if (next < 0)
                next = 0;
        if (next > 8)
                next = 8;

        if (magnitudeNext > magnitudePrev)
                return inbetweenCharsRising[prev];
        else if (magnitudeNext < magnitudePrev)
                return inbetweenCharsFalling[prev];
        else
                return upwardMotionCharsBraille[prev];
}

char *getInbetweenChar(float prev, float next)
{
        int firstDecimalDigit = (int)(fmod(prev * 10, 10));
        int secondDecimalDigit = (int)(fmod(next * 10, 10));

        return getInbetweendMotionChar(prev, next, firstDecimalDigit, secondDecimalDigit);
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

void printSpectrum(int height, int numBars, float *magnitudes, PixelData color, int indentation, bool useConfigColors, int visualizerColorType, int brailleMode)
{
        printf("\n");

        PixelData tmp;

        for (int j = height; j > 0; j--)
        {
                printf("\r");
                printBlankSpaces(indentation);
                if (color.r != 0 || color.g != 0 || color.b != 0)
                {
                        if (!useConfigColors && (visualizerColorType == 0 || visualizerColorType == 2 || visualizerColorType == 3))
                        {
                                if (visualizerColorType == 0)
                                {
                                        tmp = increaseLuminosity(color, round(j * height * 4));
                                }
                                else if (visualizerColorType == 2)
                                {
                                        tmp = increaseLuminosity(color, round((height - j) * height * 4));
                                }
                                else if (visualizerColorType == 3)
                                {
                                        tmp = getGradientColor(color, j, height, 1, 0.6f);
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

                        if (i == 0 && brailleMode)
                        {
                                printf(" ");
                        }
                        else if (i > 0 && brailleMode)
                        {
                                if (magnitudes[i - 1] >= j)
                                {
                                        printf("%s", getUpwardMotionChar(10, brailleMode));
                                }
                                else if (magnitudes[i - 1] + 1 >= j)
                                {
                                        printf("%s", getInbetweenChar(magnitudes[i - 1], magnitudes[i]));
                                }
                                else
                                {
                                        printf(" ");
                                }
                        }

                        if (!brailleMode)
                        {
                                printf(" ");
                        }

                        if (magnitudes[i] >= j)
                        {
                                printf("%s", getUpwardMotionChar(10, brailleMode));
                                if (fatBars)
                                        printf("%s", getUpwardMotionChar(10, brailleMode));
                        }
                        else if (magnitudes[i] + 1 >= j)
                        {
                                int firstDecimalDigit = (int)(fmod(magnitudes[i] * 10, 10));
                                printf("%s", getUpwardMotionChar(firstDecimalDigit, brailleMode));
                                if (fatBars)
                                        printf("%s", getUpwardMotionChar(firstDecimalDigit, brailleMode));

                        }
                        else
                        {
                                printf(" ");
                                if (fatBars)
                                        printf(" ");
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
        if (fftPreviousInput != NULL)
        {
                free(fftPreviousInput);
                fftPreviousInput = NULL;
        }
        if (fftOutput != NULL)
        {
                fftwf_free(fftOutput);
                fftOutput = NULL;
        }
}

void drawSpectrumVisualizer(AppState *state, int indentation)
{
        int height = state->uiSettings.visualizerHeight;
        PixelData color;
        color.r = state->uiSettings.color.r;
        color.g = state->uiSettings.color.g;
        color.b = state->uiSettings.color.b;

        int numBars = state->uiState.numProgressBars;
        bool useConfigColors = state->uiSettings.useConfigColors;
        int visualizerColorType = state->uiSettings.visualizerColorType;
        bool brailleMode = state->uiSettings.visualizerBrailleMode;
        tweenFactor = state->uiSettings.tweenFactor;
        tweenFactorFall = state->uiSettings.tweenFactorFall;
        elevateBarsOnSnare = state->uiSettings.elevateBarsOnSnare;
        fatBars = state->uiSettings.fatBars;

        height = height - 1;

        if (height <= 0 || numBars <= 0)
        {
                return;
        }

        if (fatBars)
                numBars *= 0.67f;

        if (numBars > MAX_BARS)
                numBars = MAX_BARS;

        if (FFT_SIZE != prevBufferSize)
        {
                lastMax = -1.0f;

                freeVisuals();

                memset(smoothedFramesMagnitudes, 0, sizeof(smoothedFramesMagnitudes));

                fftInput = (float *)malloc(sizeof(float) * FFT_SIZE);
                if (fftInput == NULL)
                {
                        for (int i = 0; i <= height; i++)
                        {
                                printf("\n");
                        }
                        return;
                }

                fftPreviousInput = (float *)malloc(sizeof(float) * FFT_SIZE);
                if (fftPreviousInput == NULL)
                {
                        fftwf_free(fftInput);
                        fftInput = NULL;
                        for (int i = 0; i <= height; i++)
                        {
                                printf("\n");
                        }
                        return;
                }

                for (int i = 0; i < FFT_SIZE; i++)
                {
                        fftPreviousInput[i] = 0.0f;
                }

                fftOutput = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * FFT_SIZE);
                if (fftOutput == NULL)
                {
                        fftwf_free(fftInput);
                        fftwf_free(fftPreviousInput);
                        fftInput = NULL;
                        fftPreviousInput = NULL;
                        for (int i = 0; i <= height; i++)
                        {
                                printf("\n");
                        }
                        return;
                }
                prevBufferSize = FFT_SIZE;
        }

        fftwf_plan plan = fftwf_plan_dft_r2c_1d(FFT_SIZE,
                                                fftInput,
                                                fftOutput,
                                                FFTW_ESTIMATE);

        float magnitudes[numBars];

        calcSpectrum(height, numBars, fftInput, fftOutput, magnitudes, plan);

        printSpectrum(height, numBars, smoothedFramesMagnitudes, color, indentation, useConfigColors, visualizerColorType, brailleMode);

        fftwf_destroy_plan(plan);
}
