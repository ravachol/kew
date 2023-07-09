#include "visuals.h"
#include "albumart.h"

PixelData increaseLuminosity(PixelData pixel, int amount) {
    PixelData pixel2;
    pixel2.r = pixel.r + amount <= 255 ? pixel.r + amount : 255;
    pixel2.g = pixel.g + amount <= 255 ? pixel.g + amount : 255;
    pixel2.b = pixel.b + amount <= 255 ? pixel.b + amount : 255;

    return pixel2;
}

PixelData decreaseLuminosity(PixelData pixel, int amount) {
    PixelData pixel2;
    pixel2.r = pixel.r - amount >= amount ? pixel.r - amount : amount;
    pixel2.g = pixel.g - amount >= amount ? pixel.g - amount : amount;
    pixel2.b = pixel.b - amount >= amount ? pixel.b - amount : amount;

    return pixel2;
}

void drawEqualizer(int height, int width, PixelData c)
{
    PixelData color;
    color.r = c.r;
    color.g = c.g;
    color.b = c.b;

    width = (width / 2);
    height = height - 1;
    
    if (height <= 0 || width <= 0)
        return;

    fftwf_complex *fftInput = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * BUFFER_SIZE);
    if (fftInput == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory for fftInput\n");
        return; 
    }

    fftwf_complex *fftOutput = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * BUFFER_SIZE);
    if (fftOutput == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory for fftOutput\n");
        fftwf_free(fftInput); 
        return; 
    }        

    fftwf_plan plan = fftwf_plan_dft_1d(BUFFER_SIZE, fftInput, fftOutput, FFTW_FORWARD, FFTW_ESTIMATE);

    float magnitudes[width];

for (int i = 0; i < BUFFER_SIZE; i++)
{
    if (g_audioBuffer == NULL) {
        fftwf_destroy_plan(plan);
        fftwf_free(fftInput);
        fftInput = NULL;
        fftwf_free(fftOutput);
        fftOutput = NULL;
        return;
    }
    ma_int32 sample = g_audioBuffer[i];
    float normalizedSample = (float)(sample & 0xFFFFFF) / 8388607.0f;  // Normalize the lower 24 bits to the range [-1, 1]
    fftInput[i][0] = normalizedSample;
    fftInput[i][1] = 0;
}

    // Perform FFT
    fftwf_execute(plan);

    int term_w, term_h;
    getTermSize(&term_w, &term_h);

    int numBins = width;
    float frequencyResolution = SAMPLE_RATE / BUFFER_SIZE;

    for (int i = 0; i < width; i++)
    {
        magnitudes[i] = 0.0f;
    }

    // Define the frequency ranges for different sections
    float lowEnd = 100.0f;
    float middleEnd = 3000.0f;
    float highEnd = 10000.0f;

    float scaleFactorLow = 1.0f;    // Scaling factor for low-end frequencies
    float scaleFactorMiddle = 1.0f; // Scaling factor for middle-end frequencies
    float scaleFactorHigh = 1.0f;   // Scaling factor for high-end frequencies
    float scaleFactor = 1.0;

    for (int i = numBins; i >= 0; i--)
    {
        float frequency = (numBins - i - 1) * frequencyResolution;
        int barIndex = 0;

        if (frequency < lowEnd)
        {

            barIndex = floor(((frequency / lowEnd) * (width)));
            scaleFactor = scaleFactorLow;
        }
        else if (frequency < middleEnd)
        {
            barIndex = (int)(((frequency - lowEnd) / (middleEnd - lowEnd)) * (width));
            scaleFactor = scaleFactorMiddle;
        }
        else if (frequency < highEnd)
        {
            barIndex = (int)(((frequency - middleEnd) / (highEnd - middleEnd)) * (width));
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
    printf("\n");
    fflush(stdout);
    float percentage = 0.3;
    float maxMagnitude = 100.0f;

    float ceiling = 1000.0f;

    for (int i = 1; i < width; i++)
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
    float exponent = 1.0;

    for (int j = height; j > 0; j--)
    {
        printf("\r");

        if (color.r != 0 || color.g != 0 ||color.b != 0)
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
            float normalizedMagnitude = magnitudes[i] / maxMagnitude;
            float scaledMagnitude = pow(normalizedMagnitude, exponent);
            float heightVal = scaledMagnitude * height;

            int barHeight = (int)round(heightVal);
            if (j >= 0)
            {
                if (barHeight >= j)
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

    fftwf_destroy_plan(plan);
    fftwf_free(fftInput);
    fftInput = NULL;
    fftwf_free(fftOutput);
    fftOutput = NULL;    
}