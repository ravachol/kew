#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "metadata.h"

extern void printHelp();

extern void printVersion(const char* version);

extern int getYear(const char* dateString);

extern void printBasicMetadata(const char *file_path);

extern void printProgress(double elapsed_seconds, double total_seconds);