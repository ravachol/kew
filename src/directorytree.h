#ifndef DIRECTORYTREE_H
#define DIRECTORYTREE_H

#include <ctype.h>
#include <dirent.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "file.h"
#include "utils.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#ifndef FILE_SYSTEM_ENTRY
#define FILE_SYSTEM_ENTRY
typedef struct FileSystemEntry
{
        int id;
        char *name;
        char *fullPath;
        int isDirectory; // 1 for directory, 0 for file
        int isEnqueued;
        int parentId;
        struct FileSystemEntry *parent;
        struct FileSystemEntry *children;
        struct FileSystemEntry *next; // For siblings (next node in the same directory)
} FileSystemEntry;
#endif

#ifndef SLOWLOADING_CALLBACK
#define SLOWLOADING_CALLBACK
typedef void (*SlowloadingCallback)(void);
#endif

FileSystemEntry *createDirectoryTree(const char *startPath, int *numEntries);

void freeTree(FileSystemEntry *root);

void freeAndWriteTree(FileSystemEntry *root, const char *filename);

FileSystemEntry *reconstructTreeFromFile(const char *filename, const char *startMusicPath, int *numDirectoryEntries);

void fuzzySearchRecursive(FileSystemEntry *node, const char *searchTerm, int threshold, void (*callback)(FileSystemEntry *, int));

void copyIsEnqueued(FileSystemEntry *library, FileSystemEntry *temp);

#endif
