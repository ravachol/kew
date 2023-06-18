#ifndef ALBUMART_H
#define ALBUMART_H
#include <stdbool.h>
#include <dirent.h>
#include "../include/imgtotxt/write_ascii.h"

extern int isAudioFile(const char *filename);

extern void addSlashIfNeeded(char *str);

extern int compareEntries(const struct dirent **a, const struct dirent **b);

extern char *findFirstPathWithAudioFile(const char *path);

extern char *findLargestImageFile(const char *directoryPath);

int displayAlbumArt(const char *filepath, int asciiHeight, int asciiWidth, bool coverBlocks, PixelData *brightPixel);

int calcIdealImgSize(int *width, int *height, const int equalizerHeight, const int metatagHeight, bool firstSong);

#endif