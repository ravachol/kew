/**
 * @file visuals.[c]
 * @brief Audio visualization rendering.
 *
 * Implements ASCII or terminal-based visualizers that react
 * to playback data such as amplitude or frequency spectrum.
 */

#include "common_ui.h"

#include "visuals.h"

#include "common/appstate.h"

#include "sound/soundcommon.h"

#include "utils/term.h"

#include <float.h>
#include <fftw3.h>
#include <math.h>
#include <complex.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_BARS 64

static float *fftInput = NULL;
static fftwf_complex *fftOutput = NULL;
static ma_format format = ma_format_unknown;
static ma_uint32 sampleRate = 0;
static float barHeight[MAX_BARS] = {0.0f};
static float displayMagnitudes[MAX_BARS] = {0.0f};
static float magnitudes[MAX_BARS] = {0.0f};
static float dBFloor = -60.0f;
static float dBCeil = -18.0f;
static float emphasis = 1.3f;
static float fastAttack = 0.6f;
static float decay = 0.14f;
static float slowAttack = 0.15f;
static int visualizerBarWidth = 2;
static int maxThinBarsInAutoMode = 20;
static int prevFftSize = 0;

void clearMagnitudes(int numBars, float *magnitudes)
{
        for (int i = 0; i < numBars; i++)
        {
                magnitudes[i] = 0.0f;
        }
}

void applyBlackmanHarris(float *fftInput, int bufferSize)
{
        if (!fftInput ||
            bufferSize < 2) // Must be at least 2 to avoid division by zero
                return;

        const float alpha0 = 0.35875f;
        const float alpha1 = 0.48829f;
        const float alpha2 = 0.14128f;
        const float alpha3 = 0.01168f;

        float denom = (float)(bufferSize - 1);

        for (int i = 0; i < bufferSize; i++)
        {
                float fraction = (float)i / denom;
                float window = alpha0 - alpha1 * cosf(2.0f * M_PI * fraction) +
                               alpha2 * cosf(4.0f * M_PI * fraction) -
                               alpha3 * cosf(6.0f * M_PI * fraction);

                fftInput[i] *= window;
        }
}

// Fill center freqs for 1/3-octave bands, given min/max freq and numBands
void computeBandCenters(float minFreq, float sampleRate, int numBands,
                        float *centerFreqs)
{
        if (!centerFreqs || numBands <= 0 || minFreq <= 0 || sampleRate <= 0)
                return;

        float nyquist = sampleRate * 0.5f;
        float octaveFraction = 1.0f / 3.0f; // 1/3 octave
        float factor = powf(2.0f, octaveFraction);
        float f = minFreq;

        // Ensure we don't exceed the Nyquist frequency
        for (int i = 0; i < numBands; i++)
        {
                if (f > nyquist)
                {
                        centerFreqs[i] =
                            nyquist; // Clamp remaining bands at Nyquist
                }
                else
                {
                        centerFreqs[i] = f;
                        f *= factor;

                        // Safeguard against overflow in case 'f' grows too
                        // large
                        if (f > nyquist)
                        {
                                f = nyquist;
                        }
                }
        }
}

void fillEQBands(const fftwf_complex *fftOutput, int bufferSize,
                 float sampleRate, float *bandDb, int numBands,
                 const float *centerFreqs)
{
        // Basic input checks
        if (!fftOutput || !bandDb || !centerFreqs || bufferSize <= 0 ||
            numBands <= 0 || sampleRate <= 0.0f)
                return;

        // Guard against potential overflow in bin count computation
        if (bufferSize > INT_MAX - 1)
                return;

        int numBins = bufferSize / 2 + 1;

        // Safe binSpacing computation
        float binSpacing = sampleRate / (float)bufferSize;
        if (binSpacing <= 0.0f || !isfinite(binSpacing))
                return;

        // Prevent division by zero in normalization
        float normFactor = (float)bufferSize;
        if (normFactor <= 0.0f)
                return;

        // Frequency window width for 1/3-octave bands
        const float width = powf(2.0f, 1.0f / 6.0f); // +/-1/6 octave

        // Pink noise correction
        const float correctionPerOctave = 3.0f;
        const float maxFreqForCorrection = 10000.0f;
        const float nyquist = sampleRate * 0.5f;

        // Make sure referenceFreq is safe
        float referenceFreq = fmaxf(centerFreqs[0], 1.0f);
        if (!isfinite(referenceFreq) || referenceFreq <= 0.0f)
                referenceFreq = 1.0f;

        for (int i = 0; i < numBands; i++)
        {
                float center = centerFreqs[i];

                if (!isfinite(center) || center <= 0.0f || center > nyquist)
                {
                        bandDb[i] = -INFINITY;
                        continue;
                }

                float lo = center / width;
                float hi = center * width;

                // Avoid integer overflows in bin index computation
                int binLo = (int)ceilf(lo / binSpacing);
                int binHi = (int)floorf(hi / binSpacing);

                binLo = (binLo < 0) ? 0 : binLo;
                binHi = (binHi >= numBins) ? numBins - 1 : binHi;
                binHi = (binHi < binLo) ? binLo : binHi;

                float sumSq = 0.0f;
                int count = 0;

                for (int k = binLo; k <= binHi; k++)
                {
                        if (k < 0 || k >= numBins)
                                continue;

                        float real = fftOutput[k][0] / normFactor;
                        float imag = fftOutput[k][1] / normFactor;
                        float mag = sqrtf(real * real + imag * imag);
                        sumSq += mag * mag;
                        count++;
                }

                float rms = (count > 0) ? sqrtf(sumSq / count)
                                        : 1e-9f; // Small nonzero floor
                bandDb[i] = 20.0f * log10f(rms);

                // Pink noise EQ compensation
                float freq = fminf(center, maxFreqForCorrection);
                float octavesAboveRef = log2f(freq / referenceFreq);
                float correction =
                    fmaxf(octavesAboveRef, 0.0f) * correctionPerOctave;
                bandDb[i] += correction;
        }
}

int normalizeAudioSamples(const void *audioBuffer, float *fftInput,
                          int bufferSize, int bitDepth)
{
        if (bitDepth == 8)
        {
                const uint8_t *buf = (const uint8_t *)audioBuffer;
                for (int i = 0; i < bufferSize; ++i)
                        fftInput[i] = ((float)buf[i] - 127.0f) / 128.0f;
        }
        else if (bitDepth == 16)
        {
                const int16_t *buf = (const int16_t *)audioBuffer;
                for (int i = 0; i < bufferSize; ++i)
                        fftInput[i] = (float)buf[i] / 32768.0f;
        }
        else if (bitDepth == 24)
        {
                const uint8_t *buf = (const uint8_t *)audioBuffer;
                for (int i = 0; i < bufferSize; ++i)
                {
                        int32_t sample = unpack_s24(buf + i * 3);
                        fftInput[i] = (float)sample / 8388608.0f;
                }
        }
        else if (bitDepth == 32)
        {
                const float *buf = (const float *)audioBuffer;
                for (int i = 0; i < bufferSize; ++i)
                        fftInput[i] = buf[i];
        }
        else
        {
                // Unsupported bit depth
                return -1;
        }

        return 0;
}

void calcMagnitudes(int height, int numBars, void *audioBuffer, int bitDepth,
                    float *fftInput, fftwf_complex *fftOutput, int fftSize,
                    float *magnitudes, fftwf_plan plan,
                    float *displayMagnitudes)
{
        // Only execute when we get the signal that we have enough samples
        // (fftSize)
        if (!isBufferReady())
                return;

        if (!audioBuffer)
        {
                fprintf(stderr, "Audio buffer is NULL.\n");
                return;
        }

        setBufferReady(false);

        normalizeAudioSamples(audioBuffer, fftInput, fftSize, bitDepth);

        // Apply Blackman Harris window function
        applyBlackmanHarris(fftInput, fftSize);

        // Compute fast fourier transform
        fftwf_execute(plan);

        // Clear previous magnitudes
        clearMagnitudes(MAX_BARS, magnitudes);

        float centerFreqs[numBars];

        float minFreq = 25.0f;
        float audibleHalf = 10000.0f;
        float maxFreq = fmin(audibleHalf, 0.5f * sampleRate);
        float octaveFraction = 1.0f / 3.0f;
        int usedBars = floor(log2(maxFreq / minFreq) / octaveFraction) +
                       1; // How many bars are actually in use, given we
                          // increase with 1/3 octave per bar

        // Compute center frequencies for EQ bands
        computeBandCenters(minFreq, maxFreq, numBars, centerFreqs);

        // Fill magnitudes for EQ bands from FFT output
        fillEQBands(fftOutput, fftSize, sampleRate, magnitudes, numBars,
                    centerFreqs);

        // Map magnitudes (in dB) to bar heights with gating and emphasis
        // (pow/gated)
        for (int i = 0; i < usedBars; ++i)
        {
                float db = magnitudes[i];
                if (db < dBFloor)
                        db = dBFloor;
                if (db > dBCeil)
                        db = dBCeil;
                float ratio = (db - dBFloor) / (dBCeil - dBFloor);
                ratio = powf(ratio, emphasis);
                if (ratio < 0.1f)
                        barHeight[i] = 0.0f; // Gate out tiny bars
                else
                        barHeight[i] = ratio * height;
        }

        float snapThreshold = 0.2f * height;

        // Smoothly update display magnitudes with attack/decay and snap
        // threshold
        for (int i = 0; i < usedBars; ++i)
        {
                float current = displayMagnitudes[i];
                float target = barHeight[i];
                float delta = target - current;
                if (delta > snapThreshold)
                        displayMagnitudes[i] +=
                            delta * fastAttack; // SNAP on big hits
                else if (delta > 0)
                        displayMagnitudes[i] += delta * slowAttack;
                else
                        displayMagnitudes[i] += delta * decay;
        }
}

char *upwardMotionCharsBlock[] = {" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};

char *upwardMotionCharsBraille[] = {" ", "⣀", "⣀", "⣤", "⣤",
                                    "⣶", "⣶", "⣿", "⣿"};

char *inbetweenCharsRising[] = {" ", "⣠", "⣠", "⣴", "⣴", "⣾", "⣾", "⣿", "⣿"};

char *inbetweenCharsFalling[] = {" ", "⡀", "⡀", "⣄", "⣄", "⣦", "⣦", "⣷", "⣷"};

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

char *getInbetweendMotionChar(float magnitudePrev, float magnitudeNext,
                              int prev, int next)
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

        return getInbetweendMotionChar(prev, next, firstDecimalDigit,
                                       secondDecimalDigit);
}

int getBitDepth(ma_format format)
{

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

        return bitDepth;
}

void printSpectrum(int row, int col, UISettings *ui, int height, int numBars,
                   int visualizerWidth, float *magnitudes)
{
        PixelData color;

        if (ui->colorMode == COLOR_MODE_ALBUM)
        {
                color.r = ui->color.r;
                color.g = ui->color.g;
                color.b = ui->color.b;
        }
        else if (ui->colorMode == COLOR_MODE_THEME && ui->theme.trackview_visualizer.type == COLOR_TYPE_RGB)
        {
                color.r = ui->theme.trackview_visualizer.rgb.r;
                color.g = ui->theme.trackview_visualizer.rgb.g;
                color.b = ui->theme.trackview_visualizer.rgb.b;
        }

        int visualizerColorType = ui->visualizerColorType;
        bool brailleMode = ui->visualizerBrailleMode;

        PixelData tmp;

        bool isPlaying = !(isPaused() || isStopped());

        for (int j = height; j > 0 && !isPlaying; j--)
        {
                printf("\033[%d;%dH", row, col);
                clearRestOfLine();
        }

        for (int j = height; j > 0 && isPlaying; j--)
        {
                printf("\033[%d;%dH", row + height - j, col);

                if (color.r != 0 || color.g != 0 || color.b != 0)
                {
                        if ((visualizerColorType == 0 ||
                             visualizerColorType == 2 ||
                             visualizerColorType == 3))
                        {
                                if (visualizerColorType == 0)
                                {
                                        tmp = increaseLuminosity(
                                            color, round(j * height * 4));
                                }
                                else if (visualizerColorType == 2)
                                {
                                        tmp = increaseLuminosity(
                                            color,
                                            round((height - j) * height * 4));
                                }
                                else if (visualizerColorType == 3)
                                {
                                        tmp = getGradientColor(color, j, height,
                                                               1, 0.6f);
                                }
                        }
                }

                if (ui->colorMode == COLOR_MODE_ALBUM)
                        printf("\033[38;2;%d;%d;%dm", tmp.r, tmp.g, tmp.b);
                else if (ui->theme.trackview_visualizer.type == COLOR_TYPE_RGB)
                {
                        printf("\033[38;2;%d;%d;%dm", tmp.r, tmp.g, tmp.b);
                }
                else
                        applyColor(ui->colorMode, ui->theme.trackview_visualizer, tmp);

                for (int i = 0; i < numBars; i++)
                {
                        if (ui->colorMode != COLOR_MODE_DEFAULT && visualizerColorType == 1)
                        {
                                tmp = (PixelData){
                                    color.r / 2, color.g / 2,
                                    color.b /
                                        2}; // Make colors half as bright before
                                            // increasing brightness
                                tmp = increaseLuminosity(
                                    tmp, round(magnitudes[i] * 10 * 4));

                                printf("\033[38;2;%d;%d;%dm", tmp.r, tmp.g,
                                       tmp.b);
                        }

                        if (i == 0 && brailleMode)
                        {
                                printf(" ");
                        }
                        else if (i > 0 && brailleMode)
                        {
                                if (magnitudes[i - 1] >= j)
                                {
                                        printf("%s", getUpwardMotionChar(
                                                         10, brailleMode));
                                }
                                else if (magnitudes[i - 1] + 1 >= j)
                                {
                                        printf("%s", getInbetweenChar(
                                                         magnitudes[i - 1],
                                                         magnitudes[i]));
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
                                printf("%s",
                                       getUpwardMotionChar(10, brailleMode));
                                if (visualizerBarWidth == 1 ||
                                    (visualizerBarWidth == 2 &&
                                     visualizerWidth > maxThinBarsInAutoMode))
                                        printf("%s", getUpwardMotionChar(
                                                         10, brailleMode));
                        }
                        else if (magnitudes[i] + 1 >= j)
                        {
                                int firstDecimalDigit =
                                    (int)(fmod(magnitudes[i] * 10, 10));
                                printf("%s",
                                       getUpwardMotionChar(firstDecimalDigit,
                                                           brailleMode));
                                if (visualizerBarWidth == 1 ||
                                    (visualizerBarWidth == 2 &&
                                     visualizerWidth > maxThinBarsInAutoMode))
                                        printf("%s", getUpwardMotionChar(
                                                         firstDecimalDigit,
                                                         brailleMode));
                        }
                        else
                        {
                                printf(" ");
                                if (visualizerBarWidth == 1 ||
                                    (visualizerBarWidth == 2 &&
                                     visualizerWidth > maxThinBarsInAutoMode))
                                        printf(" ");
                        }
                }
        }
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

void drawSpectrumVisualizer(int row, int col, int height)
{
        AppState *state = getAppState();
        
        int numBars = state->uiState.numProgressBars;
        int visualizerWidth = state->uiState.numProgressBars;
        visualizerBarWidth = state->uiSettings.visualizerBarWidth;

        if (visualizerBarWidth == 1 ||
            (visualizerBarWidth == 2 &&
             visualizerWidth > maxThinBarsInAutoMode))
                numBars *= 0.67f;

        height -= 1;

        if (height <= 0 || numBars <= 0)
        {
                return;
        }

        if (numBars > MAX_BARS)
                numBars = MAX_BARS;

        int fftSize = getFftSize();

        if (fftSize != prevFftSize)
        {
                freeVisuals();

                memset(displayMagnitudes, 0, sizeof(displayMagnitudes));

                fftInput = (float *)malloc(sizeof(float) * fftSize);
                if (fftInput == NULL)
                {
                        for (int i = 0; i <= height; i++)
                        {
                                printf("\n");
                        }
                        return;
                }

                fftOutput = (fftwf_complex *)fftwf_malloc(
                    sizeof(fftwf_complex) * fftSize);
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
                prevFftSize = fftSize;
        }

        fftwf_plan plan =
            fftwf_plan_dft_r2c_1d(fftSize, fftInput, fftOutput, FFTW_ESTIMATE);

        getCurrentFormatAndSampleRate(&format, &sampleRate);

        int bitDepth = getBitDepth(format);

        calcMagnitudes(height, numBars, getAudioBuffer(), bitDepth, fftInput,
                       fftOutput, fftSize, magnitudes, plan, displayMagnitudes);

        printSpectrum(row, col, &(state->uiSettings), height, numBars,
                      visualizerWidth, displayMagnitudes);

        fftwf_destroy_plan(plan);
}
