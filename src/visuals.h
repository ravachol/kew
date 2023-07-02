#include <math.h>
#include <float.h>
#include <fftw3.h>
#include "soundgapless.h"
#include "term.h"
#include "write_ascii.h"
#define SAMPLE_RATE 44100
#define BUFFER_SIZE 1024

void drawEqualizer(int height, int width, PixelData color);

PixelData increaseLuminosity(PixelData pixel, int amount);

PixelData decreaseLuminosity(PixelData pixel, int amount);