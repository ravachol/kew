#ifndef ALBUMART_H
#define ALBUMART_H
#include <stdbool.h>
#include <dirent.h>
#include "../include/imgtotxt/write_ascii.h"

int displayAlbumArt(const char *filepath, int asciiHeight, int asciiWidth, bool coverBlocks, PixelData *brightPixel);

int calcIdealImgSize(int *width, int *height, const int equalizerHeight, const int metatagHeight, bool firstSong);

#endif