#ifndef FILE_H
#define FILE_H

#include <stdbool.h>

#define __USE_GNU

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

#ifndef AUDIO_EXTENSIONS
#define AUDIO_EXTENSIONS "(m4a|aac|mp3|ogg|flac|wav|opus|webm)$"
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
