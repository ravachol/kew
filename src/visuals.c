#include "visuals.h"
#include "albumart.h"
#define SAMPLE_RATE 192000
#define BUFFER_SIZE 1024
#define CHANNELS 2

// Beat-detection related
#define FRAME_SIZE 4410
#define HOP_SIZE 2205 
#define WINDOW_SIZE 256
#define BEAT_THRESHOLD 0.2
#define EXAGGERATE_AMOUNT 2.3
#define SMOOTHING_WINDOW_SIZE 100
#define MAX_NUM_BEATS 100
#define MINIMUM_INTERVAL_BEAT 200
float magnitudeBuffer[WINDOW_SIZE] = {0.0f};
int bufferIndex = 0; 

float* calculateEnergy(float* audio, int audioLength, int frameSize, int hopSize)
{
    int numFrames = (audioLength - frameSize) / hopSize + 1;
    float* energy = malloc(numFrames * sizeof(float));
  
    for (int i = 0; i < numFrames; i++)
    {
        float frameEnergy = 0.0f;
        int frameStart = i * hopSize;
      
        for (int j = 0; j < frameSize; j++)
        {
            float sample = audio[frameStart + j];
            frameEnergy += sample * sample;
        }
      
        energy[i] = frameEnergy;
    }
    return energy;
}

void smoothEnergy(float* energy, int length, int windowSize, float* smoothedEnergy)
{
    for (int i = 0; i < length; i++)
    {
        int start = i - windowSize / 2;
        int end = i + windowSize / 2;
        start = (start < 0) ? 0 : start;
        end = (end >= length) ? length - 1 : end;

        float sum = 0.0f;
        for (int j = start; j <= end; j++)
        {
            sum += energy[j];
        }
        smoothedEnergy[i] = sum / (end - start + 1);
    }
}

int* findPeaks(float* energy, int size, float threshold, int* numPeaks) {
    int* peaks = malloc(size * sizeof(int));
    int peakIndex = 0;

    for (int i = 1; i < size - 1; i++) {
        if (energy[i] > threshold && energy[i] > energy[i-1] && energy[i] > energy[i+1]) {
            peaks[peakIndex++] = i; 
        }
    }

    *numPeaks = peakIndex;

    return peaks;
}

void refineBeats(int* beats, int numBeats, int minimumInterbeatInterval, int* refinedBeats, int* numRefinedBeats)
{
    int lastBeat = -INT_MAX;
    int refinedBeatIndex = 0;

    for (int i = 0; i < numBeats; i++)
    {
        int beat = beats[i];

        if (beat - lastBeat >= minimumInterbeatInterval)
        {
            refinedBeats[refinedBeatIndex] = beat;
            refinedBeatIndex++;
            lastBeat = beat;
        }
    }

    *numRefinedBeats = refinedBeatIndex;
}

void updateMagnitudeBuffer(float magnitude)
{
    magnitudeBuffer[bufferIndex] = magnitude;
    bufferIndex = (bufferIndex + 1) % WINDOW_SIZE;  // Update the buffer index in a circular manner
}

float calculateMovingAverage()
{
    float sum = 0.0f;
    for (int i = 0; i < WINDOW_SIZE; i++)
    {
        sum += magnitudeBuffer[i];
    }
    return sum / WINDOW_SIZE;
}

float calculateThreshold()
{
    float sum = 0.0f;
    for (int i = 0; i < WINDOW_SIZE; i++)
    {
        sum += magnitudeBuffer[i];
    }
    float mean = sum / WINDOW_SIZE;

    float variance = 0.0f;
    for (int i = 0; i < WINDOW_SIZE; i++)
    {
        float diff = magnitudeBuffer[i] - mean;
        variance += diff * diff;
    }
    variance /= WINDOW_SIZE;

    float stdDeviation = sqrtf(variance);

    float threshold = mean + (stdDeviation * BEAT_THRESHOLD);

    return threshold;
}

int detectBeats(float* magnitudes, int numBars)
{
    float avgMagnitude = 0.0f;
    int range = 5;
    range = (range > numBars) ? numBars : range;

    for (int i = 0; i < range; i++)
    {
        float magnitude = magnitudes[i];
        avgMagnitude += magnitude;
    }
    avgMagnitude /= range;

    updateMagnitudeBuffer(avgMagnitude);

    float movingAverage = calculateMovingAverage();
    float threshold = calculateThreshold();

    float* energy = calculateEnergy(magnitudeBuffer, WINDOW_SIZE, FRAME_SIZE, HOP_SIZE);

    float* smoothedEnergy = (float*)malloc(WINDOW_SIZE * sizeof(float));
    smoothEnergy(energy, WINDOW_SIZE, SMOOTHING_WINDOW_SIZE, smoothedEnergy);

    int numPeaks = 0;
    int* peaks = findPeaks(smoothedEnergy, WINDOW_SIZE, threshold, &numPeaks);

    int* refinedBeats = (int*)malloc(MAX_NUM_BEATS * sizeof(int));
    int numRefinedBeats = 0;
    refineBeats(peaks, numPeaks, MINIMUM_INTERVAL_BEAT, refinedBeats, &numRefinedBeats);

    int peakDetected = 0;

    if (numRefinedBeats > 0) {
        peakDetected = 1;
    }

    for (int i = 0; i < WINDOW_SIZE; i++)
    {
        if (smoothedEnergy[i] >= threshold)
        {
            peakDetected = 1;
            break;
        }
    }

    free(energy);
    free(smoothedEnergy);
    free(peaks);
    free(refinedBeats);

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

void updateMagnitudes(int height, int width, float maxMagnitude, fftwf_complex* fftOutput, float* magnitudes) {
    float exponent = 1.0;
    float exaggerationFactor = 1.0;

    int beat = detectBeats(magnitudes, width);
    if (beat > 0) {
        exaggerationFactor = EXAGGERATE_AMOUNT;
    }

    for (int i = 0; i < width; i++) {
        float normalizedMagnitude = magnitudes[i] / maxMagnitude;
        float scaledMagnitude = pow(normalizedMagnitude, exponent);
        magnitudes[i] = scaledMagnitude * height * exaggerationFactor;
    }
}

float calcMaxMagnitude(int numBars, float *magnitudes)
{
    float percentage = 0.3;
    float maxMagnitude = 0.0f;
    float ceiling = 300.0f;
    float threshold = ceiling * percentage;
    for (int i = 1; i < numBars; i++)
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
    if (g_audioBuffer == NULL) {
        return;
    }

    for (int i = 0; i < BUFFER_SIZE; i++)
    {
        ma_int32 sample = g_audioBuffer[i];
        float normalizedSample = (float)(sample & 0xFFFFFF) / 8388607.0f;  // Normalize the lower 24 bits to the range [-1, 1]
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
                color = increaseLuminosity(color , 60);
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
    if (fftInput == NULL) {
        return; 
    }

    fftwf_complex *fftOutput = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * BUFFER_SIZE);
    if (fftOutput == NULL) {
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
