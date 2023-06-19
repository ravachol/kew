#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "metadata.h"
#include "common.h"

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

void printHelp();

void printAsciiLogo();

void printVersion(const char *version, const char *latestVersion);

int getYear(const char *dateString);