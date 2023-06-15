#ifndef ALBUMART_H
#define ALBUMART_H
#include <stdbool.h>

extern int default_ascii_height;
extern int default_ascii_width;

extern int small_ascii_height;
extern int small_ascii_width;

extern int isAudioFile(const char* filename);

extern void addSlashIfNeeded(char* str);

extern int compareEntries(const struct dirent **a, const struct dirent **b) ;

extern char* findFirstPathWithAudioFile(const char* path);

extern char* findLargestImageFile(const char* directoryPath);

int displayAlbumArt(const char* filepath, int asciiHeight, int asciiWidth, bool coverBlocks);

int calcIdealImgSize(int* width, int* height, const int visualizationHeight, const int metatagHeight);

#endif