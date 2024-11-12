

#include <float.h>
#include <fftw3.h>
#include <math.h>
#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include "sound.h"
#include "term.h"
#include "utils.h"

#ifndef PIXELDATA_STRUCT
#define PIXELDATA_STRUCT
    typedef struct
    {
        unsigned char r;
        unsigned char g;
        unsigned char b;
    } PixelData;
#endif

void initVisuals(void);

void freeVisuals(void);

void drawSpectrumVisualizer(int height, int width, PixelData c, int indentation, bool useConfigColors);

PixelData increaseLuminosity(PixelData pixel, int amount);
