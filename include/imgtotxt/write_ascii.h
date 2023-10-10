#ifndef C_H
#define C_H
#include <stdbool.h>
#include "options.h"

/* This ifdef allows the header to be used from both C and C++. */
#ifdef __cplusplus
extern "C"
{
#endif
#ifndef PIXELDATA_STRUCT
#define PIXELDATA_STRUCT
    typedef struct
    {
        unsigned char r;
        unsigned char g;
        unsigned char b;
    } PixelData;
#endif
    int getBrightPixel(char *filepath, int width, int height, PixelData *brightPixel);
    int output_ascii(char *pathToImgFile, int height, int width, PixelData *brightPixel);
#ifdef __cplusplus
}
#endif

#endif
