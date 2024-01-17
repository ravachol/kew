#ifndef FILE_H
#define FILE_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <stdio.h>
#include <regex.h>
#include <pwd.h>
#include <sys/types.h>
#include "stringfunc.h"
#include "cutils.h"

#define RETRY_DELAY_MILLISECONDS 100
#define MAX_RETRY_COUNT 20
#define MAX_FILENAME_LENGTH 256

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

#ifndef AUDIO_EXTENSIONS
#define AUDIO_EXTENSIONS "\\.(m4a|mp3|ogg|flac|wav|aac|wma|raw|mp4a|mp4|m4p|opus)$"
#endif

enum SearchType
{
        SearchAny = 0,
        DirOnly = 1,
        FileOnly = 2,
        SearchPlayList = 3,
        ReturnAllSongs = 4
};

int existsFile(const char *fname);

void getDirectoryFromPath(const char *path, char *directory);

int tryOpen(const char *path);

int isDirectory(const char *path);

/* Traverse a directory tree and search for a given file or directory */
int walker(const char *startPath, const char *searching, char *result,
           const char *allowedExtensions, enum SearchType searchType, bool exactSearch);

int expandPath(const char *inputPath, char *expandedPath);

int createDirectory(const char *path);

int removeDirectory(const char *path);

int deleteFile(const char *filePath);

char *escapeFilePath(const char *filePath);

void generateTempFilePath(const char *srcFilePath, char *filePath, const char *prefix, const char *suffix);

const char *getFileExtension(const char *filePath);

int openFileWithRetry(const char *filePath, const char *mode, FILE **file);

void deleteTempDir(void);

void deleteTempShmDir(void);

#endif
