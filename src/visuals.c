#include "visuals.h"
#include "albumart.h"
#define SAMPLE_RATE 192000
#define BUFFER_SIZE 1024
#define CHANNELS 2

// Beat-detection related
#define WINDOW_SIZE 512
#define BEAT_THRESHOLD 0.3
#define MAGNITUDE_CEIL 300
#define JUMP_AMOUNT 2.4
int bufferIndex = 0;

float magnitudeBuffer[WINDOW_SIZE] = {0.0f};

void updateMagnitudeBuffer(float magnitude)
{
    magnitudeBuffer[bufferIndex] = magnitude;
    bufferIndex = (bufferIndex + 1) % WINDOW_SIZE; // Update the buffer index in a circular manner
}

float calculateMovingAverage()
{
    float sum = 0.0f;
    int numSamples = 0;    
    for (int i = 0; i < WINDOW_SIZE; i++) 
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
    for (int i = 0; i < WINDOW_SIZE; i++) 
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
    for (int i = 1; i < WINDOW_SIZE; i++)
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

    int numSamples = 0;
    for (int i = 1; i < range; i++)
    {
        float magnitude = magnitudes[i];
        avgMagnitude += magnitude;
        if (magnitude != 0.0f)
        {
            numSamples++;
        }
    }
    numSamples = (numSamples > 0) ? numSamples : 1;
    avgMagnitude /= numSamples;

    updateMagnitudeBuffer(avgMagnitude);

    float movingAverage = calculateMovingAverage();
    float threshold = calculateThreshold(); 

    int peakDetected = 0;
    for (int i = 1; i < range; i++)
    {
        if (magnitudes[i] > threshold)
        {
            peakDetected = 1;
            break;
        }
    }

    if (peakDetected)
    {
        // Beat detected
        return 1;
    }
    else
    {
        return 0;
    }
}

void updateMagnitudes(int height, int width, float maxMagnitude, fftwf_complex *fftOutput, float *magnitudes)
{
    float exponent = 1.0;
    float jumpFactor = 1.0;

    int beat = detectBeats(magnitudes, width);
    if (beat > 0)
    {
        jumpFactor = JUMP_AMOUNT;
    }

    for (int i = 0; i < width; i++)
    {
        float normalizedMagnitude = magnitudes[i] / maxMagnitude;
        float scaledMagnitude = pow(normalizedMagnitude, exponent);
        magnitudes[i] = scaledMagnitude * height * jumpFactor;
    }
}

float lastMax = MAGNITUDE_CEIL / 2;
float calcMaxMagnitude(int numBars, float *magnitudes)
{
    float percentage = 0.3;
    float maxMagnitude = 0.0f;
    float ceiling = MAGNITUDE_CEIL;
    float threshold = ceiling * percentage;
    for (int i = 1; i < numBars; i++) // We start at 1 because 0 is always some wildly large number for some reason
    {
        if (magnitudes[i] > maxMagnitude)
        {
            maxMagnitude = magnitudes[i];
        }
    }
    if (maxMagnitude > ceiling)
    {
        maxMagnitude = ceiling;
    }
    if (maxMagnitude < threshold)
    {
        maxMagnitude = threshold;
    }
    maxMagnitude = (maxMagnitude + lastMax) / 2;
    lastMax = maxMagnitude;
    return maxMagnitude;
}

void clearMagnitudes(int width, float *magnitudes)
{
    for (int i = 0; i < width; i++)
    {
        magnitudes[i] = 0.0f;
    }
}

void calcSpectrum(int height, int width, fftwf_complex *fftInput, fftwf_complex *fftOutput, float *magnitudes, fftwf_plan plan)
{
    if (g_audioBuffer == NULL)
    {
        return;
    }

    for (int i = 0; i < BUFFER_SIZE; i++)
    {
        ma_int32 sample = g_audioBuffer[i];
        float normalizedSample = (float)(sample & 0xFFFFFF) / 8388607.0f; // Normalize the lower 24 bits to the range [-1, 1]
        fftInput[i][0] = normalizedSample;
        fftInput[i][1] = 0;
    }

    fftwf_execute(plan);
    clearMagnitudes(width, magnitudes);

    for (int i = 0; i < width; i++)
    {
        float magnitude = sqrtf(fftOutput[i][0] * fftOutput[i][0] + fftOutput[i][1] * fftOutput[i][1]);
        magnitudes[i] += magnitude;
    }

    float maxMagnitude = calcMaxMagnitude(width, magnitudes);
    updateMagnitudes(height, width, maxMagnitude, fftOutput, magnitudes);
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

void printSpectrum(int height, int width, PixelData c, float *magnitudes, PixelData color)
{
    printf("\n");
    fflush(stdout);
    clearRestOfScreen();

    for (int j = height; j > 0; j--)
    {
        printf("\r");

        if (color.r != 0 || color.g != 0 || color.b != 0)
        {
            if (j == height)
            {
                printf("\033[38;2;%d;%d;%dm", 255, 255, 255);
            }
            else if (j == height - 1)
            {
                color = increaseLuminosity(color, 60);
                printf("\033[38;2;%d;%d;%dm", color.r, color.g, color.b);
            }
            else
            {
                color = decreaseLuminosity(color, 25);
                printf("\033[38;2;%d;%d;%dm", color.r, color.g, color.b);
            }
        }
        else
        {
            setDefaultTextColor();
        }
        for (int i = 1; i < width; i++)
        {
            if (j >= 0)
            {
                if ((int)round(magnitudes[i]) >= j)
                {
                    printf(" █");
                }
                else
                {
                    printf("  ");
                }
            }
        }
        printf("\n ");
    }
    printf("\r");
    color = decreaseLuminosity(color, 25);
    printf("\033[38;2;%d;%d;%dm", color.r, color.g, color.b);
    for (int i = 1; i < width; i++)
    {
        printf(" █");
    }
    printf("\n");
    printf("\r");
    fflush(stdout);
}

void drawSpectrumVisualizer(int height, int width, PixelData c, ma_int32 *audioData)
{
    PixelData color;
    color.r = c.r;
    color.g = c.g;
    color.b = c.b;

    width = (width / 2) + 1; // We only print a bar every other column and the first is always maxed so it is ignored.
                             // Therefore we need to add 1.
    height = height - 1;

    if (height <= 0 || width <= 0)
    {
        return;
    }

    fftwf_complex *fftInput = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * BUFFER_SIZE);
    if (fftInput == NULL)
    {
        return;
    }

    fftwf_complex *fftOutput = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * BUFFER_SIZE);
    if (fftOutput == NULL)
    {
        fftwf_free(fftInput);
        return;
    }

    fftwf_plan plan = fftwf_plan_dft_1d(BUFFER_SIZE, fftInput, fftOutput, FFTW_FORWARD, FFTW_ESTIMATE);

    float magnitudes[width];

    calcSpectrum(height, width, fftInput, fftOutput, magnitudes, plan);
    printSpectrum(height, width, color, magnitudes, color);

    fftwf_destroy_plan(plan);
    fftwf_free(fftInput);
    fftInput = NULL;
    fftwf_free(fftOutput);
    fftOutput = NULL;
}
