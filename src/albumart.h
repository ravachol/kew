#ifndef ALBUMART_H
#define ALBUMART_H
#include <stdbool.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/param.h>
#include "../include/imgtotxt/write_ascii.h"
#include "songloader.h"

int extractCoverCommand(const char *inputFilePath, const char *outputFilePath);

char *findLargestImageFileRecursive(const char *directoryPath, char *largestImageFile, off_t *largestFileSize);

int displayAlbumArt(const char *filepath, int width, int height, bool coverBlocks, PixelData *brightPixel);

int calcIdealImgSize(int *width, int *height, const int equalizerHeight, const int metatagHeight);

int displayCover(SongData *songdata, int width, int height, bool ascii);

#endif