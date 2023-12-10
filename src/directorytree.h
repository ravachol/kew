#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/types.h>
#include <regex.h>
#include "file.h"
#include "stringfunc.h"
#include "albumart.h"

#ifndef FILE_SYSTEM_ENTRY
#define FILE_SYSTEM_ENTRY
typedef struct FileSystemEntry
{
        char name[256];
        char fullPath[PATH_MAX]; 
        int isDirectory; // 1 for directory, 0 for file
        int isEnqueued;
        struct FileSystemEntry *parent;
        struct FileSystemEntry *children;
        struct FileSystemEntry *next; // For siblings (next node in the same directory)        
} FileSystemEntry;
#endif

FileSystemEntry *createDirectoryTree(const char *startPath, int *numEntries);
void freeTree(FileSystemEntry *root);