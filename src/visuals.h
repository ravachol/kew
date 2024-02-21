

#include <float.h>
#include <fftw3.h>
#include <math.h>
#include <locale.h>
#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include "sound.h"
#include "term.h"

void initVisuals();

void freeVisuals();

void drawSpectrumVisualizer(int height, int width, PixelData c, int indentation, bool useProfileColors);

PixelData increaseLuminosity(PixelData pixel, int amount);

void printBlankSpaces(int numSpaces);
