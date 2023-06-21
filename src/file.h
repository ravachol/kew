#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#define RETRY_DELAY_MICROSECONDS 100000
#define MAX_RETRY_COUNT 20

int deleteFile(const char *filePath);

void generateTempFilePath(char *filePath, const char *prefix, const char *suffix);

const char *getFileExtension(const char *filePath);

int openFileWithRetry(const char *filePath, const char *mode, FILE **file);