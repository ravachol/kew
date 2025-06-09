

#include "appstate.h"

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

void drawSpectrumVisualizer(int row, int col, AppState *state);
