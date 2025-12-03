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
typedef struct FileSystemEntry {
        int id;
        char *name;
        char *full_path;
        int is_directory;
        int is_enqueued;
        int parent_id;
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

FileSystemEntry *create_directory_tree(const char *start_path, int *num_entries);
void free_tree(FileSystemEntry *root);
FileSystemEntry *read_tree_from_binary(
    const char *filename,
    const char *start_music_path,
    int *num_directory_entries, bool set_enqueued_status);
int write_tree_to_binary(FileSystemEntry *root, const char *filename);
void fuzzy_search_recursive(FileSystemEntry *node, const char *search_term,
                            int threshold,
                            void (*callback)(FileSystemEntry *, int));
void copy_is_enqueued(FileSystemEntry *library, FileSystemEntry *tmp);
void sort_file_system_tree(FileSystemEntry *root, int (*comparator)(const void *, const void *));
int compare_folders_by_age_files_alphabetically(const void *a, const void *b);
int compare_lib_entries(const struct dirent **a, const struct dirent **b);
int compare_lib_entries_reversed(const struct dirent **a, const struct dirent **b);
int compare_entry_natural_reversed(const void *a, const void *b);
int compare_entry_natural(const void *a, const void *b);
FileSystemEntry *find_corresponding_entry(FileSystemEntry *tmp, const char *full_path);


#endif
