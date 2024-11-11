#ifndef C_H
#define C_H
#include <stdbool.h>

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
    int printInAscii(const char *pathToImgFile, int height, int width);
#ifdef __cplusplus
}
#endif

#endif
