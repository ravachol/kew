#ifndef DIR_H
#define DIR_H
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <stdio.h>
#include <regex.h>
#include <sys/param.h>
#include "stringextensions.h"
#define MAX_FILENAME_LENGTH 256

enum SearchType {Any = 0, DirOnly = 1, FileOnly = 2}; 

void getDirectoryFromPath(const char* path, char* directory);

int tryOpen(const char* path);

int isDirectory(const char* path);

// Function to traverse a directory tree and search for a given file or directory
extern int walker(const char *startPath, const char *searching, char *result,
           const char *allowedExtensions, enum SearchType searchType);

#endif           