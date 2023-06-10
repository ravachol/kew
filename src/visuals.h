#include <math.h>
#include <float.h>
#include <fftw3.h>
#include "sound.h"
#include "term.h"
#define SAMPLE_RATE 44100
#define BUFFER_SIZE 1024

int extendRange = 8;

void drawSpectrum(int height, int width) 
{
    width = width * extendRange;
    fftwf_complex* fftInput = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * BUFFER_SIZE);
    fftwf_complex* fftOutput = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * BUFFER_SIZE);

    fftwf_plan plan = fftwf_plan_dft_1d(BUFFER_SIZE, fftInput, fftOutput, FFTW_FORWARD, FFTW_ESTIMATE);

    float magnitudes[width]; 

    for (int i = 0; i < BUFFER_SIZE; i++) {
        ma_int16 sample = *((ma_int16*)g_audioBuffer + i);
        float normalizedSample = (float)sample / 32767.0f;
        fftInput[i][0] = normalizedSample;
        fftInput[i][1] = 0;     
    }

    // Perform FFT
    fftwf_execute(plan);

    int term_w, term_h;
    getTermSize(&term_w, &term_h);  
    
    int binSize = BUFFER_SIZE / width;    
    int numBins = BUFFER_SIZE / 2;
    float frequencyResolution = SAMPLE_RATE / BUFFER_SIZE;

    for (int i = 0; i < width; i++) {
        magnitudes[i] = 0.0f;
    }

    for (int i = numBins - 1; i >= 0; i--) {
        float frequency = (numBins - i - 1) * frequencyResolution;
        int barIndex = ((frequency / (SAMPLE_RATE / 2)) * (width));

        float magnitude1 = sqrtf(fftOutput[i][0] * fftOutput[i][0] + fftOutput[i][1] * fftOutput[i][1]); // Magnitude of channel 1
        // Calculate the combined magnitude
        float combinedMagnitude = magnitude1;
        magnitudes[barIndex] += combinedMagnitude;             
    }

    printf("\n"); // Start on a new line 
    fflush(stdout);
    
    if (term_w < width / extendRange)
      width = term_w * extendRange;

    float maxMagnitude = 0.0f;
    float threshold = 28.0f;
    float ceiling = 80.0f;
    // Find the maximum magnitude in the current frame
    for (int i = 0; i < width; i++) {
        if (magnitudes[i] > maxMagnitude) {
            maxMagnitude = magnitudes[i];
        }
    }    

    if (maxMagnitude < threshold)
      maxMagnitude = threshold;
    if (maxMagnitude > ceiling)
      maxMagnitude = ceiling;

    for (int j = height; j > 0; j--) {
        printf("\r"); // Move cursor to the beginning of the line

        for (int i = 0; i < width / extendRange; i++) {
              float normalizedMagnitude = magnitudes[i] / maxMagnitude;

            float scaledMagnitude = normalizedMagnitude * 2;

            float heightVal = scaledMagnitude * height;
            if (heightVal > 0.5f && heightVal < 1.0f) {
                heightVal = 1.0f; // Exaggerate low values
            }
            int barHeight = (int)round(heightVal);
            if (barHeight >= j) {
                printf("║");
            } else {
                printf(" ");
            }
        }
        printf("\n");
    }  
    
    // Restore the cursor position
    printf("\033[%dA", height+1);
    fflush(stdout);

    fftwf_destroy_plan(plan);
    fftwf_free(fftInput);
    fftwf_free(fftOutput);
}