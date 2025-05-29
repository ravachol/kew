#ifndef IMGFUNC_H
#define IMGFUNC_H

#include <chafa.h>
#include <chafa-canvas-config.h>
#ifndef PIXELDATA_STRUCT
#define PIXELDATA_STRUCT
typedef struct
{
        unsigned char r;
        unsigned char g;
        unsigned char b;
} PixelData;
#endif

int printInAscii(const char *pathToImgFile, int height);

float calcAspectRatio(void);

unsigned char *getBitmap(const char *image_path, int *width, int *height);

void printSquareBitmapCentered(unsigned char *pixels, int width, int height, int baseHeight);

int getCoverColor(unsigned char *pixels, int width, int height, unsigned char *r, unsigned char *g, unsigned char *b);

#ifdef CHAFA_VERSION_1_16
gboolean retire_passthrough_workarounds_tmux(void);
#endif

#endif
