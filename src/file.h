#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

int deleteFile(const char* filePath);

void generateTempFilePath(char* filePath, const char* prefix, const char* suffix);