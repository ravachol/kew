
/**
 * @file visuals.[h]
 * @brief Audio visualization rendering.
 *
 * Implements ASCII or terminal-based visualizers that react
 * to playback data such as amplitude or frequency spectrum.
 */

#include "common/appstate.h"

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

void drawSpectrumVisualizer(int row, int col, int height, AppState *state);
