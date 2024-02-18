#ifndef ALBUMART_H
#define ALBUMART_H
#include <stdbool.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/param.h>
#include "../include/imgtotxt/write_ascii.h"
#include "songloader.h"

int extractCover(const char *inputFilePath, const char *outputFilePath);

char *findLargestImageFile(const char *directoryPath, char *largestImageFile, off_t *largestFileSize);

int displayAlbumArt(const char *filepath, int width, int height, bool coverAnsi, PixelData *brightPixel);

int calcIdealImgSize(int *width, int *height, const int visualizerHeight, const int metatagHeight);

int displayCover(SongData *songdata, int width, int height, bool ascii);

#endif
