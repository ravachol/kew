#ifndef FILE_H
#define FILE_H

#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <pwd.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#define __USE_GNU
#include <unistd.h>
#include "utils.h"

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

#ifndef AUDIO_EXTENSIONS
#define AUDIO_EXTENSIONS "(m4a|aac|mp3|ogg|flac|wav|opus)$"
#endif

enum SearchType
{
        SearchAny = 0,
        DirOnly = 1,
        FileOnly = 2,
        SearchPlayList = 3,
        ReturnAllSongs = 4
};

void getDirectoryFromPath(const char *path, char *directory);

int isDirectory(const char *path);

/* Traverse a directory tree and search for a given file or directory */
int walker(const char *startPath, const char *searching, char *result,
           const char *allowedExtensions, enum SearchType searchType, bool exactSearch);

int expandPath(const char *inputPath, char *expandedPath);

int createDirectory(const char *path);

int deleteFile(const char *filePath);

void generateTempFilePath(char *filePath, const char *prefix, const char *suffix);

int isInTempDir(const char *path);

int existsFile(const char *fname);

#endif
