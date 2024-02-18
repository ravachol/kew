#include <math.h>
#include <float.h>
#include <fftw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <locale.h>
#include <complex.h>
#include "soundgapless.h"
#include "term.h"

void initVisuals();

void freeVisuals();

void drawSpectrumVisualizer(int height, int width, PixelData c, int indentation);

PixelData increaseLuminosity(PixelData pixel, int amount);

void printBlankSpaces(int numSpaces);
