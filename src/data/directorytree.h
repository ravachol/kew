/**
 * @file directorytree.h
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
        int is_directory;
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

FileSystemEntry *create_directory_tree(const char *startPath, int *numEntries);
void free_tree(FileSystemEntry *root);
void free_and_write_tree(FileSystemEntry *root, const char *filename);
void fuzzy_search_recursive(FileSystemEntry *node, const char *searchTerm,
                          int threshold,
                          void (*callback)(FileSystemEntry *, int));
void copy_is_enqueued(FileSystemEntry *library, FileSystemEntry *tmp);
void sort_file_system_tree(FileSystemEntry *root, int (*comparator)(const void *, const void *));
int compare_folders_by_age_files_alphabetically(const void *a, const void *b);
int compare_lib_entries(const struct dirent **a, const struct dirent **b);
int compare_lib_entries_reversed(const struct dirent **a, const struct dirent **b);
int compare_entry_natural_reversed(const void *a, const void *b);
int compare_entry_natural(const void *a, const void *b);
FileSystemEntry *find_corresponding_entry(FileSystemEntry *tmp, const char *fullPath);
FileSystemEntry *reconstruct_tree_from_file(const char *filename,
                                         const char *startMusicPath,
                                         int *numDirectoryEntries);

#endif
