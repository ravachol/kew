#include <math.h>
#include <float.h>
#include <fftw3.h>
#include "sound.h"
#include "term.h"
#define SAMPLE_RATE 44100
#define BUFFER_SIZE 1024

extern int extendRange;

void drawSpectrum(int height, int width);