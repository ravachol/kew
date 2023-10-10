#include <math.h>
#include <float.h>
#include <fftw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "soundgapless.h"
#include "term.h"
#include "write_ascii.h"

void drawSpectrumVisualizer(int height, int width, PixelData c);

PixelData increaseLuminosity(PixelData pixel, int amount);

PixelData decreaseLuminosity(PixelData pixel, int amount);

void printBlankSpaces(int numSpaces);