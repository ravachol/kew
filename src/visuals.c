#include "visuals.h"
#include "albumart.h"
#include "complex.h"
#define SAMPLE_RATE 192000
#define BUFFER_SIZE 3600
#define CHANNELS 2
#define BEAT_THRESHOLD 0.3
#define MAGNITUDE_CEIL 150
#define MAGNITUDE_FLOOR_FRACTION 0.4

int bufferIndex = 0;

float magnitudeBuffer[BUFFER_SIZE] = {0.0f};
float lastMagnitudes[BUFFER_SIZE] = {0.0f};

void updateMagnitudeBuffer(float magnitude)
{
    magnitudeBuffer[bufferIndex] = magnitude;
    bufferIndex = (bufferIndex + 1) % BUFFER_SIZE;
}

float calculateMovingAverage()
{
    float sum = 0.0f;
    int numSamples = 0;
    for (int i = 0; i < BUFFER_SIZE; i++)
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
    for (int i = 0; i < BUFFER_SIZE; i++)
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
    for (int i = 0; i < BUFFER_SIZE; i++)
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
    int jumpAmount = ceil(height * 0.25);

    int beat = detectBeats(magnitudes, width);
    if (beat > 0)
    {
        jumpFactor = jumpAmount;
    }

    for (int i = 0; i < width; i++)
    {
        if (i < 3)
            exponent = 1.0;
        else
            exponent = 2.0;
                    
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

float lastMax = MAGNITUDE_CEIL / 2;
float alpha = 0.2;

float calcMaxMagnitude(int numBars, float *magnitudes)
{
    float maxMagnitude = 0.0f;
    float threshold = MAGNITUDE_CEIL * MAGNITUDE_FLOOR_FRACTION;
    for (int i = 0; i < numBars; i++)
    {
        if (magnitudes[i] > maxMagnitude)
        {
            maxMagnitude = magnitudes[i];
        }
    }
    if (maxMagnitude > MAGNITUDE_CEIL)
    {
        maxMagnitude = MAGNITUDE_CEIL;
    }
    if (maxMagnitude < threshold)
    {
        maxMagnitude = threshold;
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

void compressSpectrum(int width, fftwf_complex *fftOutput, float *compressedMagnitudes, int numBars, float fractionToKeep)
{
    if (numBars <= 0)
    {
        return;
    }

    if (fractionToKeep < 0.0f || fractionToKeep > 1.0f)
    {
        return;
    }

    int compressionFactor = width / numBars;
    int barSpan = ceil(compressionFactor * fractionToKeep);

    for (int i = 0; i < numBars; i++)
    {
        int startIndex = (int)i * barSpan;

        int barEndIndex = startIndex + barSpan;

        if (barEndIndex > width)
            barEndIndex = width;

        float barMagnitude = 0.0f;
        for (int j = startIndex; j < barEndIndex; j++)
        {
            float magnitude = cabsf(fftOutput[j][0] + fftOutput[j][1] * I);
            barMagnitude += magnitude;
        }
        barMagnitude /= (barEndIndex - startIndex); // Normalize by the number of bins
        compressedMagnitudes[i] = barMagnitude;
    }
}

void calcSpectrum(int height, int numBars, fftwf_complex *fftInput, fftwf_complex *fftOutput, float *magnitudes, fftwf_plan plan)
{
    if (g_audioBuffer == NULL)
    {
        return;
    }

    for (int i = 0; i < BUFFER_SIZE; i++)
    {
        ma_int32 sample = g_audioBuffer[i];
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
        float normalizedSample = (float)lower24Bits / 8388607.0f;
        fftInput[i][0] = normalizedSample;
        fftInput[i][1] = 0;
    }

    // Apply Windowing (Hamming Window)
    for (int i = 0; i < BUFFER_SIZE; i++)
    {
        float window = 0.54f - 0.46f * cos(2 * M_PI * i / (BUFFER_SIZE - 1));
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

    for (int j = height; j > 0; j--)
    {
        printf("\r");

        if (color.r != 0 || color.g != 0 || color.b != 0)
        {
            if (!useProfileColors)
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
            
        }
        else
        {
            setDefaultTextColor();
        }
        for (int i = 0; i < width; i++)
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
    if (!useProfileColors) printf("\033[38;2;%d;%d;%dm", color.r, color.g, color.b);
    for (int i = 0; i < width; i++)
    {
        printf(" █");
    }
    printf("\n");
    printf("\r");
    fflush(stdout);
}

void drawSpectrumVisualizer(int height, int width, PixelData c)
{
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

    float magnitudes[numBars];

    calcSpectrum(height, numBars, fftInput, fftOutput, magnitudes, plan);
    printSpectrum(height, numBars, magnitudes, color);

    fftwf_destroy_plan(plan);
    fftwf_free(fftInput);
    fftInput = NULL;
    fftwf_free(fftOutput);
    fftOutput = NULL;
}
