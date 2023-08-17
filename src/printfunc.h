#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "metadata.h"

#ifndef TAGSETTINGS_STRUCT
#define TAGSETTINGS_STRUCT

typedef struct
{
    char title[256];
    char artist[256];
    char album_artist[256];
    char album[256];
    char date[256];
} TagSettings;

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

void printHelp();

int printAsciiLogo();

void printVersion(const char *version);

void printVersionDefaultColors(const char *version, const char *versionDate);

int getYear(const char *dateString);