/**
 * @file directorytree.[h]
 * @brief Filesystem scanning and in-memory tree construction.
 *
 * Provides operations for recursively traversing directories and
 * constructing an in-memory linked tree representation of the music
 * library. Used by library and playlist modules to index songs efficiently.
 */

#ifndef DIRECTORYTREE_H
#define DIRECTORYTREE_H

#include <dirent.h>
#include <stdbool.h>

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
        int isDirectory;
        int isEnqueued;
        int parentId;
        struct FileSystemEntry *parent;
        struct FileSystemEntry *children;
        struct FileSystemEntry *next;      // For siblings (next node in the same directory)
        struct FileSystemEntry *lastChild; // TEMP: only for construction
} FileSystemEntry;
#endif

#ifndef SLOWLOADING_CALLBACK
#define SLOWLOADING_CALLBACK
typedef void (*SlowloadingCallback)(void);
#endif

FileSystemEntry *createDirectoryTree(const char *startPath, int *numEntries);

void freeTree(FileSystemEntry *root);

void freeAndWriteTree(FileSystemEntry *root, const char *filename);

FileSystemEntry *reconstructTreeFromFile(const char *filename,
                                         const char *startMusicPath,
                                         int *numDirectoryEntries);

void fuzzySearchRecursive(FileSystemEntry *node, const char *searchTerm,
                          int threshold,
                          void (*callback)(FileSystemEntry *, int));

void copyIsEnqueued(FileSystemEntry *library, FileSystemEntry *tmp);

void sortFileSystemTree(FileSystemEntry *root, int (*comparator)(const void *, const void *));

int compareFoldersByAgeFilesAlphabetically(const void *a, const void *b);

int compareLibEntries(const struct dirent **a, const struct dirent **b);

int compareLibEntriesReversed(const struct dirent **a, const struct dirent **b);

int compareEntryNaturalReversed(const void *a, const void *b);

int compareEntryNatural(const void *a, const void *b);

FileSystemEntry *findCorrespondingEntry(FileSystemEntry *tmp, const char *fullPath);

#endif
