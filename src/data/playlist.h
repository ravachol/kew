/**
 * @file playlist.h
 * @brief Playlist data structures and operations.
 *
 * Defines the core playlist data types and provides functions for managing
 * track lists, iterating songs, and maintaining play order. Used by both
 * playback and UI modules to coordinate current and upcoming tracks.
 */

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef __USE_XOPEN_EXTENDED
#define __USE_XOPEN_EXTENDED
#endif

#include "directorytree.h"

#include <pthread.h>
#include <stdbool.h>

#define MAX_FILES 50000

#ifndef PLAYLIST_STRUCT
#define PLAYLIST_STRUCT

typedef struct
{
        char *file_path;
        double duration;
} SongInfo;

typedef struct Node {
        int id;
        SongInfo song;
        struct Node *next;
        struct Node *prev;
} Node;

typedef struct
{
        Node *head;
        Node *tail;
        int count;
        pthread_mutex_t mutex;
} PlayList;

#endif

/**
 * @brief Clears the currently selected song.
 *
 * Sets the global current song pointer to NULL.
 */
void clear_current_song(void);

/**
 * @brief Sets the currently selected song.
 *
 * @param node Pointer to the node representing the song to set
 *             as the current song. May be NULL.
 */
void set_current_song(Node *node);

/**
 * @brief Gets the currently selected song.
 *
 * @return Pointer to the current song node, or NULL if none is set.
 */
Node *get_current_song(void);

/**
 * @brief Returns the next node in the playlist.
 *
 * @param node The current node.
 * @return Pointer to the next node, or NULL if node is NULL
 *         or if there is no next node.
 */
Node *get_list_next(Node *node);

/**
 * @brief Returns the previous node in the playlist.
 *
 * @param node The current node.
 * @return Pointer to the previous node, or NULL if node is NULL
 *         or if there is no previous node.
 */
Node *get_list_prev(Node *node);

/**
 * @brief Adds a node to the end of a playlist.
 *
 * @param list Pointer to the playlist.
 * @param new_node Pointer to the node to add.
 *
 * @return 0 on success,
 *        -1 if the playlist is NULL or full,
 *         0 if new_node is NULL (no insertion performed).
 *
 * @note The playlist count is incremented on successful insertion.
 */
int add_to_list(PlayList *list, Node *new_node);

/**
 * @brief Moves a node one position up in the playlist.
 *
 * @param list Pointer to the playlist.
 * @param node Pointer to the node to move.
 *
 * @note Does nothing if node is NULL, already at the head,
 *       or has no previous node.
 */
void move_up_list(PlayList *list, Node *node);

/**
 * @brief Moves a node one position down in the playlist.
 *
 * @param list Pointer to the playlist.
 * @param node Pointer to the node to move.
 *
 * @note Does nothing if node is NULL, already at the tail,
 *       or has no next node.
 */
void move_down_list(PlayList *list, Node *node);

/**
 * @brief Removes a node from the playlist and frees its memory.
 *
 * @param list Pointer to the playlist.
 * @param node Pointer to the node to remove.
 *
 * @return Pointer to the next node after the deleted node,
 *         or NULL if deletion failed.
 *
 * @note Frees the node and its associated file path string.
 *       Decrements the playlist count.
 */
Node *delete_from_list(PlayList *list, Node *node);

/**
 * @brief Removes all nodes from a playlist.
 *
 * @param list Pointer to the playlist.
 *
 * @note Frees all nodes and their file paths.
 *       Resets head, tail, and count.
 *       Uses internal mutex locking.
 */
void empty_playlist(PlayList *list);

/**
 * @brief Randomly shuffles the order of nodes in the playlist.
 *
 * @param playlist Pointer to the playlist.
 *
 * @note Uses the Fisher-Yates shuffle algorithm.
 *       Exits the program on memory allocation failure.
 */
void shuffle_playlist(PlayList *playlist);

/**
 * @brief Shuffles the playlist while keeping a specific song first.
 *
 * @param playlist Pointer to the playlist.
 * @param song Pointer to the song that should remain first after shuffle.
 *
 * @note The playlist is shuffled first, then the specified song
 *       is moved to the head if present.
 */
void shuffle_playlist_starting_from_song(PlayList *playlist, Node *song);

/**
 * @brief Creates a new playlist node.
 *
 * @param node Output pointer to the allocated node.
 * @param directory_path Path to the song file.
 * @param id Unique identifier for the node.
 *
 * @note Allocates memory for the node and duplicates the file path.
 *       Exits the program on allocation failure.
 */
void create_node(Node **node, const char *directory_path, int id);

/**
 * @brief Frees a single node and its associated resources.
 *
 * @param node Pointer to the node to destroy.
 */
void destroy_node(Node *node);

/**
 * @brief Builds a playlist recursively from a directory.
 *
 * @param directory_path Root directory path.
 * @param allowed_extensions Regular expression for allowed file extensions.
 * @param playlist Playlist to populate.
 *
 * @note Traverses subdirectories recursively.
 *       Adds matching files up to MAX_FILES.
 */
void build_playlist_recursive(const char *directory_path,
                              const char *allowed_extensions,
                              PlayList *playlist);

/**
 * @brief Reads an M3U playlist file and appends its entries to a playlist.
 *
 * @param filename Path to the M3U file.
 * @param playlist Playlist to populate.
 *
 * @return Pointer to the first node added or found,
 *         or NULL on failure.
 *
 * @note Skips duplicate entries already in the playlist.
 */
Node *read_m3u_file(const char *filename, PlayList *playlist);

/**
 * @brief Creates a playlist based on command-line arguments and search rules.
 *
 * @param playlist Pointer to the playlist pointer.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @param exact_search Whether to perform exact matching.
 * @param path Base search path.
 *
 * @return 0 on completion.
 *
 * @note May shuffle playlist depending on arguments.
 *       Prints a message if no music is found.
 */
int make_playlist(PlayList **playlist, int argc, char *argv[],
                  bool exact_search, const char *path);

/**
 * @brief Writes a playlist to an M3U file.
 *
 * @param filename Output file path.
 * @param playlist Playlist to write.
 */
void write_m3u_file(const char *filename, const PlayList *playlist);

/**
 * @brief Exports the current playlist to an M3U file.
 *
 * @param path Destination directory.
 * @param playlist Playlist to export.
 */
void export_current_playlist(const char *path,
                             const PlayList *playlist);

/**
 * @brief Loads the favorites playlist from disk.
 *
 * @param directory Directory containing the favorites file.
 * @param favorites_playlist Pointer to the playlist pointer.
 */
void load_favorites_playlist(const char *directory,
                             PlayList **favorites_playlist);

/**
 * @brief Adds enqueued songs from a file system tree to a playlist.
 *
 * @param root Root file system entry.
 * @param playlist Playlist to populate.
 */
void add_enqueued_songs_to_playlist(FileSystemEntry *root,
                                    PlayList *playlist);

/**
 * @brief Saves a playlist with a specific name.
 *
 * @param directory Target directory.
 * @param playlist_name Name of the playlist file.
 * @param playlist Playlist to save.
 */
void save_named_playlist(const char *directory,
                         const char *playlist_name,
                         const PlayList *playlist);

/**
 * @brief Saves the favorites playlist to disk.
 *
 * @param directory Target directory.
 * @param favorites_playlist Playlist to save.
 */
void save_favorites_playlist(const char *directory,
                             PlayList *favorites_playlist);

/**
 * @brief Creates a deep copy of a playlist.
 *
 * @param original_list Playlist to copy.
 *
 * @return Newly allocated deep copy, or NULL on failure.
 */
PlayList *deep_copy_playlist(const PlayList *original_list);

/**
 * @brief Copies the contents of one playlist into another.
 *
 * @param original_list Source playlist.
 * @param new_list Destination playlist pointer.
 *
 * @note Existing contents of destination are cleared.
 */
void deep_copy_play_list_onto_list(const PlayList *original_list,
                                   PlayList **new_list);

/**
 * @brief Searches for the first occurrence of a path in a playlist.
 *
 * @param path File path to search for.
 * @param playlist Playlist to search.
 *
 * @return Pointer to the matching node, or NULL if not found.
 */
Node *find_path_in_playlist(const char *path, PlayList *playlist);

/**
 * @brief Searches for the last occurrence of a path in a playlist.
 *
 * @param path File path to search for.
 * @param playlist Playlist to search.
 *
 * @return Pointer to the matching node, or NULL if not found.
 */
Node *find_last_path_in_playlist(const char *path,
                                 PlayList *playlist);

/**
 * @brief Finds a node by its ID in a playlist.
 *
 * @param list Playlist to search.
 * @param id Node identifier.
 * @param found_node Output pointer for the found node.
 *
 * @return Index of the node in the list,
 *         or -1 if not found.
 */
int find_node_in_list(PlayList *list, int id,
                      Node **found_node);

/**
 * @brief Checks if a filename has a supported music file extension.
 *
 * @param filename Name of the file.
 *
 * @return 1 if the file is a supported music file,
 *         0 otherwise.
 */
int is_music_file(const char *filename);

/**
 * @brief Creates a playlist from a file system tree.
 *
 * @param root Root file system entry.
 * @param list Playlist to populate.
 * @param playlist_max Maximum number of entries allowed.
 */
void create_play_list_from_file_system_entry(FileSystemEntry *root,
                                             PlayList *list,
                                             int playlist_max);

/**
 * @brief Adds shuffled albums from a file system tree to a playlist.
 *
 * @param root Root file system entry.
 * @param list Playlist to populate.
 * @param playlist_max Maximum number of entries allowed.
 */
void add_shuffled_albums_to_play_list(FileSystemEntry *root,
                                      PlayList *list,
                                      int playlist_max);

/**
 * @brief Increments and returns the global node ID counter.
 *
 * @return The incremented node ID.
 */
int increment_node_id(void);
