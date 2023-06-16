#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "metadata.h"

void printHelp();

void printVersion(const char *version);

int getYear(const char *dateString);

void printBasicMetadata(const char *file_path);

void printProgress(double elapsed_seconds, double total_seconds, double total_duration_seconds);