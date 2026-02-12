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

/**
 * Creates a directory tree starting at the given path.
 *
 * Recursively scans the file system beginning at @p start_path and
 * builds a tree of FileSystemEntry structures. Empty directories
 * are removed from the resulting tree.
 *
 * @param start_path  The root path from which to build the tree
 * @param num_entries Output parameter that receives the number of
 *                    directory entries found (after cleanup)
 *
 * @return Pointer to the root FileSystemEntry on success,
 *         or NULL on failure
 */
FileSystemEntry *create_directory_tree(const char *start_path, int *num_entries);

/**
 * Frees an entire FileSystemEntry tree.
 *
 * Iteratively traverses the tree and releases all associated memory,
 * including names, full paths, and node structures.
 *
 * @param root  The root node of the tree to free
 */
void free_tree(FileSystemEntry *root);

/**
 * Reads a FileSystemEntry tree from a binary database file.
 *
 * Reconstructs the tree structure from the specified file,
 * restoring parent-child relationships and full paths.
 *
 * @param filename               Path to the binary database file
 * @param start_music_path       Fallback root path if needed
 * @param num_directory_entries  Output parameter that receives the
 *                               number of directory entries (may be NULL)
 * @param set_enqueued_status    Whether to restore the stored
 *                               is_enqueued flags
 *
 * @return Pointer to the reconstructed root node on success,
 *         or NULL on failure
 */
FileSystemEntry *read_tree_from_binary(
    const char *filename,
    const char *start_music_path,
    int *num_directory_entries,
    bool set_enqueued_status);

/**
 * Writes a FileSystemEntry tree to a binary database file.
 *
 * Flattens the tree structure and serializes all entries,
 * including metadata and string tables, into the specified file.
 *
 * @param root      The root of the tree to serialize
 * @param filename  Path to the output binary file
 *
 * @return 0 on success, -1 on failure
 */
int write_tree_to_binary(FileSystemEntry *root, const char *filename);

/**
 * Recursively performs a fuzzy search on a FileSystemEntry tree.
 *
 * Traverses the tree and applies a fuzzy matching algorithm
 * to each node's name. If a match within the given threshold
 * is found, the provided callback function is invoked.
 *
 * @param node        The current node to evaluate
 * @param search_term The search string
 * @param threshold   Maximum allowed distance for a match
 * @param callback    Function invoked for each matching entry,
 *                    receiving the node and its match distance
 */
void fuzzy_search_recursive(FileSystemEntry *node,
                            const char *search_term,
                            int threshold,
                            void (*callback)(FileSystemEntry *, int));

/**
 * Copies the is_enqueued status from one tree to another.
 *
 * Traverses the source library tree and propagates the
 * is_enqueued flag to corresponding entries in the
 * target tree based on full path matching.
 *
 * @param library  Source tree containing the enqueue states
 * @param tmp      Target tree to receive the enqueue states
 */
void copy_is_enqueued(FileSystemEntry *library, FileSystemEntry *tmp);

/**
 * Sorts a FileSystemEntry tree recursively.
 *
 * Applies the given comparator to the children of each directory
 * node and recursively sorts all subdirectories.
 *
 * @param root        Root of the tree to sort
 * @param comparator  Comparison function compatible with qsort
 */
void sort_file_system_tree(FileSystemEntry *root,
                           int (*comparator)(const void *, const void *));

/**
 * Comparator that sorts directories by modification time (newest first)
 * and files alphabetically. Directories are placed before files.
 *
 * @param a  Pointer to first FileSystemEntry pointer
 * @param b  Pointer to second FileSystemEntry pointer
 *
 * @return Negative, zero, or positive value as required by qsort
 */
int compare_folders_by_age_files_alphabetically(const void *a,
                                                const void *b);

/**
 * Comparator for directory entries using natural order comparison.
 *
 * Performs a case-insensitive natural sort (numbers compared numerically).
 * Entries beginning with '_' are placed after other entries.
 *
 * @param a  Pointer to first struct dirent pointer
 * @param b  Pointer to second struct dirent pointer
 *
 * @return Negative, zero, or positive value as required by scandir
 */
int compare_lib_entries(const struct dirent **a,
                        const struct dirent **b);

/**
 * Reversed variant of compare_lib_entries().
 *
 * @param a  Pointer to first struct dirent pointer
 * @param b  Pointer to second struct dirent pointer
 *
 * @return Inverted comparison result of compare_lib_entries
 */
int compare_lib_entries_reversed(const struct dirent **a,
                                 const struct dirent **b);

/**
 * Comparator for FileSystemEntry pointers using natural order,
 * reversed (descending).
 *
 * @param a  Pointer to first FileSystemEntry pointer
 * @param b  Pointer to second FileSystemEntry pointer
 *
 * @return Negative, zero, or positive value as required by qsort
 */
int compare_entry_natural_reversed(const void *a, const void *b);

/**
 * Comparator for FileSystemEntry pointers using natural order.
 *
 * Performs a case-insensitive natural sort (numbers compared numerically).
 * Entries beginning with '_' are placed after other entries.
 *
 * @param a  Pointer to first FileSystemEntry pointer
 * @param b  Pointer to second FileSystemEntry pointer
 *
 * @return Negative, zero, or positive value as required by qsort
 */
int compare_entry_natural(const void *a, const void *b);

/**
 * Finds a corresponding entry in a tree by full path.
 *
 * Recursively searches the tree for a node whose full_path
 * matches the specified string.
 *
 * @param tmp        Root node of the tree to search
 * @param full_path  Full path string to match
 *
 * @return Pointer to the matching FileSystemEntry,
 *         or NULL if not found
 */
FileSystemEntry *find_corresponding_entry(FileSystemEntry *tmp,
                                          const char *full_path);

#endif
