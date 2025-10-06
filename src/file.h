#ifndef FILE_H
#define FILE_H

#include <stdbool.h>
#include "common.h" 

#define __USE_GNU

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

#ifndef MUSIC_FILE_EXTENSIONS
#define MUSIC_FILE_EXTENSIONS "(m4a|aac|mp3|ogg|flac|wav|opus|webm)$"
#endif

#ifndef AUDIO_EXTENSIONS
#define AUDIO_EXTENSIONS "(m4a|aac|mp3|ogg|flac|wav|opus|webm|m3u|m3u8)$"
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

Lyrics *load_lyrics(const char *music_file_path);
void free_lyrics(Lyrics *lyrics);


#endif