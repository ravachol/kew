#include "visuals.h"
#include "albumart.h"

void drawEqualizer(int height, int width)
{
    width =  width / 2;
    height = height - 2;
    width++;
    if (height <= 0 || width <= 0)
        return;    

    fftwf_complex *fftInput = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * BUFFER_SIZE);
    fftwf_complex *fftOutput = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * BUFFER_SIZE);

    fftwf_plan plan = fftwf_plan_dft_1d(BUFFER_SIZE, fftInput, fftOutput, FFTW_FORWARD, FFTW_ESTIMATE);

    float magnitudes[width];

    for (int i = 0; i < BUFFER_SIZE; i++)
    {
        ma_int16 sample = *((ma_int16 *)g_audioBuffer + i);
        float normalizedSample = (float)sample / 32767.0f;
        fftInput[i][0] = normalizedSample;
        fftInput[i][1] = 0;
    }

    // Perform FFT
    fftwf_execute(plan);

    int term_w, term_h;
    getTermSize(&term_w, &term_h);

    int binSize = BUFFER_SIZE / width;
    int numBins = width;
    float frequencyResolution = SAMPLE_RATE / BUFFER_SIZE;

    for (int i = 0; i < width; i++)
    {
        magnitudes[i] = 0.0f;
    }

    // Define the frequency ranges for different sections
    float lowEnd = 100.0f;
    float middleEnd = 1000.0f;
    float highEnd = 10000.0f;

    float scaleFactorLow = 1.4f;    // Scaling factor for low-end frequencies
    float scaleFactorMiddle = 1.7f; // Scaling factor for middle-end frequencies
    float scaleFactorHigh = 1.7f;   // Scaling factor for high-end frequencies
    float scaleFactor = 1.0;

    for (int i = numBins; i >= 0; i--)
    {
        float frequency = (numBins - i - 1) * frequencyResolution;
        int barIndex = 0;

        if (frequency < lowEnd)
        {
            // Low-end frequency range
            barIndex = floor(((frequency / lowEnd) * (width)));
            // Apply scaling factor for low-end frequencies
            scaleFactor = scaleFactorLow;
        }
        else if (frequency < middleEnd)
        {
            // Middle-end frequency range
            barIndex = (int)(((frequency - lowEnd) / (middleEnd - lowEnd)) * (width));
            // Apply scaling factor for middle-end frequencies
            scaleFactor = scaleFactorMiddle;
        }
        else if (frequency < highEnd)
        {
            // High-end frequency range
            barIndex = (int)(((frequency - middleEnd) / (highEnd - middleEnd)) * (width));
            // Apply scaling factor for high-end frequencies
            scaleFactor = scaleFactorHigh;
        }

        if (barIndex < 0)
            barIndex = 0;
        if (barIndex >= width)
            barIndex = width;

        float magnitude1 = sqrtf(fftOutput[i][0] * fftOutput[i][0] + fftOutput[i][1] * fftOutput[i][1]);
        float combinedMagnitude = magnitude1;
        magnitudes[i] += combinedMagnitude * scaleFactor;
    }
    printf("\n"); // Start on a new line
    fflush(stdout);
    float percentage = 0.3;
    float maxMagnitude = 0.0f;

    float ceiling = 100.0f;
    // Find the maximum magnitude in the current frame
    for (int i = 0; i < width; i++)
    {
        if (magnitudes[i] > maxMagnitude)
        {
            maxMagnitude = magnitudes[i];
        }
    }
    float threshold = percentage * maxMagnitude;
    if (maxMagnitude < threshold)
        maxMagnitude = threshold;
    if (maxMagnitude > ceiling)
        maxMagnitude = ceiling;

    clearRestOfScreen();
    float exponent = 0.8;
    int hasDrawn[width];

    for (int i = 0; i < width; i++)
    {
        hasDrawn[i] = 0;
    }
    for (int j = height; j > 0; j--)
    {
        printf("\r"); // Move cursor to the beginning of the line

        for (int i = 0; i < width; i++)
        {
            float normalizedMagnitude = magnitudes[i] / maxMagnitude;
            // float scaledMagnitude = normalizedMagnitude * scaleFactor;
            float scaledMagnitude = pow(normalizedMagnitude, exponent);
            float heightVal = scaledMagnitude * height;

            int barHeight = (int)round(heightVal);
            if (j >= 0)
                if (barHeight >= j && i > 0) 
                {
                    if (j == 0&& hasDrawn[i] == 0)
                    {
                        printf("╔╗");
                        hasDrawn[i] = 1;
                    }
                    else if (hasDrawn[i] == 0)
                    {
                        printf("╔╗");
                        hasDrawn[i] = 1;
                    }
                    else
                    {
                        printf("║║");
                        hasDrawn[i] = 1;
                    }
                }
                else if (i != 0)
                {
                    printf("  ");
                }
        }
        printf("\033[0m\n"); // Reset the color and move to the next line
    }
    printf("\r");    
    for (int i = 1; i < width; i++)
    {
        if (hasDrawn[i] == 0)
            printf("╔╗");
        else
            printf("║║");
    }
    printf("\033[0m\n"); // Reset the color and move to the next line
    printf("\r");
    for (int i = 1; i < width; i++)
    {
            printf("╚╝"); 
    }
    // printf("  ");
    //  Restore the cursor position
    printf("\033[%dA", height + 2);
    printf("\r");

    fflush(stdout);

    fftwf_destroy_plan(plan);
    fftwf_free(fftInput);
    fftwf_free(fftOutput);
}