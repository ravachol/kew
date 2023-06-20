#ifndef C_H
#define C_H
#include <stdbool.h>
#include "options.h"

/* This ifdef allows the header to be used from both C and C++. */
#ifdef __cplusplus
extern "C"
{
#endif
    typedef struct
    {
        unsigned char r;
        unsigned char g;
        unsigned char b;
    } PixelData;

    int getBrightPixel(char *filepath, int width, int height, PixelData *brightPixel);
    int output_ascii(char *pathToImgFile, int height, int width, bool coverBlocks, PixelData *brightPixel);
#ifdef __cplusplus
}
#endif

#endif
