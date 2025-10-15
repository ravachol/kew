/**
 * @file imgfunc.h
 * @brief Image rendering and conversion helpers using Chafa.
 *
 * Handles loading and displaying album art or other images in the terminal
 * using the Chafa library. Supports scaling, aspect-ratio preservation,
 * and different rendering modes (truecolor, ASCII, etc.).
 */

#ifndef IMGFUNC_H
#define IMGFUNC_H

#include <chafa.h>
#include <chafa-canvas-config.h>

int printInAsciiCentered(const char *pathToImgFile, int height);
int printInAscii(int indentation, const char *pathToImgFile, int height);
int getCoverColor(unsigned char *pixels, int width, int height, unsigned char *r, unsigned char *g, unsigned char *b);
float getAspectRatio();
float calcAspectRatio(void);
unsigned char *getBitmap(const char *image_path, int *width, int *height);
void printSquareBitmapCentered(unsigned char *pixels, int width, int height, int baseHeight);
void printSquareBitmap(int row, int col, unsigned char *pixels, int width, int height, int baseHeight);

#ifdef CHAFA_VERSION_1_16
gboolean retirePassthroughWorkarounds_tmux(void);
#endif

#endif
