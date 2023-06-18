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

void printHelp();

void printVersion(const char *version, const char *latestVersion);

int getYear(const char *dateString);

TagSettings printBasicMetadata(const char *file_path);

void printProgress(double elapsed_seconds, double total_seconds, double total_duration_seconds);