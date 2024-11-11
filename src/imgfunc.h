#include <chafa.h>
#include <chafa-canvas-config.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

float calcAspectRatio(void);

unsigned char *getBitmap(const char *image_path, int *width, int *height);

void printSquareBitmapCentered(unsigned char *pixels, int width, int height, int baseHeight);

int getCoverColor(unsigned char *pixels, int width, int height, unsigned char *r, unsigned char *g, unsigned char *b);
