/**
 * @file library_ops.h
 * @brief Music library management and scanning operations.
 *
 * Responsible for reading directories, and updating the in-memory library
 * representation used by playlists
 * and the UI browser.
 */

#ifndef LIBRARY_OPS_H
#define LIBRARY_OPS_H

#include "common/appstate.h"

/**
 * @brief Create and initialize the music library.
 *
 * Attempts to load the library from a cached binary representation and
 * optionally restores the enqueued status of entries. If changes are detected
 * in the underlying filesystem, the library may be rebuilt.
 *
 * @param set_enqueued_status If true, restore enqueued status from the cached
 *                            library representation.
 */
void create_library(bool set_enqueued_status);

/**
 * @brief Update the library by rescanning the given path.
 *
 * Spawns a background thread to rebuild the directory tree from the specified
 * path. Optionally blocks until the update is complete.
 *
 * @param path Filesystem path to scan for music files.
 * @param wait_until_complete If true, blocks until the update thread finishes.
 */
void update_library(char *path, bool wait_until_complete);

/**
 * @brief Prompt the user to decide whether to cache the current library.
 *
 * Typically used to ask for confirmation before writing a cached binary
 * representation of the library to disk.
 */
void ask_if_cache_library(void);

/**
 * @brief Toggle the library sorting mode.
 *
 * Switches between natural sorting and an alternate sorting mode
 * (e.g., folders by age and files alphabetically), then refreshes the UI.
 */
void sort_library(void);

/**
 * @brief Reset the library sorting to the default (natural) order.
 *
 * If a custom sorting mode is active, restores natural sorting.
 */
void reset_sort_library(void);

/**
 * @brief Mark all entries contained in a playlist as enqueued.
 *
 * Traverses the library tree and sets the enqueued flag for entries whose
 * paths appear in the given playlist. The root itself is not marked.
 *
 * @param root Root of the library tree.
 * @param playlist Playlist containing songs to mark as enqueued.
 */
void mark_list_as_enqueued(FileSystemEntry *root, PlayList *playlist);

/**
 * @brief Enqueue a single song into the active playlists.
 *
 * Adds the given file entry to both the unshuffled and shuffled playlists
 * and marks it (and its parent) as enqueued.
 *
 * @param child FileSystemEntry representing the song to enqueue.
 */
void enqueue_song(FileSystemEntry *child);

/**
 * @brief Dequeue a single song from the active playlists.
 *
 * Removes the song from both playlists, updates playback state if necessary,
 * and clears its enqueued flag.
 *
 * @param child FileSystemEntry representing the song to dequeue.
 */
void dequeue_song(FileSystemEntry *child);

/**
 * @brief Recursively dequeue all songs under a directory.
 *
 * Traverses all descendants of the given directory entry and removes
 * each song from the playlists. Updates parent enqueued states accordingly.
 *
 * @param parent Directory whose children should be dequeued.
 */
void dequeue_children(FileSystemEntry *parent);

/**
 * @brief Update parent enqueued states based on children.
 *
 * Recursively propagates the desired enqueued status up the directory tree,
 * ensuring that parent directories reflect the state of their children.
 *
 * @param parent The parent entry to update.
 * @param wanted_status The desired enqueued status.
 */
void set_childrens_queued_status_on_parents(FileSystemEntry *parent, bool wanted_status);

/**
 * @brief Recursively enqueue all songs under a directory.
 *
 * Traverses the subtree starting at the given entry and enqueues all
 * non-playlist song files. Optionally returns the first enqueued entry.
 *
 * @param child Root of the subtree to enqueue.
 * @param first_enqueued_entry Output parameter that receives the first
 *                             enqueued FileSystemEntry, if any.
 *
 * @return 1 if at least one entry was enqueued, 0 otherwise.
 */
int enqueue_children(FileSystemEntry *child,
                     FileSystemEntry **first_enqueued_entry);

/**
 * @brief Mark a specific entry as dequeued by path.
 *
 * Recursively searches the tree for the entry matching the given path,
 * clears its enqueued flag, and updates parent flags if necessary.
 *
 * @param root Root of the library tree.
 * @param path Full path of the entry to dequeue.
 *
 * @return true if the entry was found and dequeued, false otherwise.
 */
bool mark_as_dequeued(FileSystemEntry *root, char *path);

/**
 * @brief Determine whether an entry has any direct song children.
 *
 * Checks if the given directory contains at least one non-directory child.
 *
 * @param entry Directory entry to inspect.
 *
 * @return true if at least one song child exists, false otherwise.
 */
bool has_song_children(FileSystemEntry *entry);

/**
 * @brief Determine whether a directory contains any dequeued descendants.
 *
 * Recursively checks whether any child (or descendant) song entry is not
 * currently enqueued.
 *
 * @param parent Directory entry to inspect.
 *
 * @return true if any child or descendant is dequeued, false otherwise.
 */
bool has_dequeued_children(FileSystemEntry *parent);

/**
 * @brief Check whether an entry is contained within another entry.
 *
 * Traverses the parent chain of the given entry to determine whether it
 * is a descendant of the specified containing entry.
 *
 * @param entry The entry to test.
 * @param containing_entry The potential ancestor entry.
 *
 * @return true if @p entry is contained within @p containing_entry,
 *         false otherwise.
 */
bool is_contained_within(FileSystemEntry *entry,
                         FileSystemEntry *containing_entry);

/**
 * @brief Update the library if filesystem changes are detected.
 *
 * Checks the modification time of the library root and its top-level
 * subdirectories. If changes are detected since the last run, triggers
 * a library update in a background thread.
 *
 * @param wait_until_complete If true, blocks until the update finishes.
 */
void update_library_if_changed_detected(bool wait_until_complete);

#endif
