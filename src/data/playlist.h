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

Node *get_current_song(void);
Node *get_list_next(Node *node);
Node *get_list_prev(Node *node);
Node *delete_from_list(PlayList *list, Node *node);
Node *find_path_in_playlist(const char *path, PlayList *playlist);
Node *find_last_path_in_playlist(const char *path, PlayList *playlist);
PlayList *deep_copy_playlist(const PlayList *original_list);
int increment_node_id(void);
int add_to_list(PlayList *list, Node *new_node);
int make_playlist(PlayList **playlist, int argc, char *argv[], bool exact_search, const char *path);
int find_node_in_list(PlayList *list, int id, Node **found_node);
int is_music_file(const char *filename);
void clear_current_song(void);
void set_current_song(Node *node);
void create_node(Node **node, const char *directory_path, int id);
void empty_playlist(PlayList *list);
void destroy_node(Node *node);
void shuffle_playlist(PlayList *playlist);
void shuffle_playlist_starting_from_song(PlayList *playlist, Node *song);
void write_m3u_file(const char *filename, const PlayList *playlist);
void export_current_playlist(const char *path, const PlayList *playlist);
void load_favorites_playlist(const char *directory, PlayList **favorites_playlist);
void add_enqueued_songs_to_playlist(FileSystemEntry *root, PlayList *playlist);
void save_named_playlist(const char *directory, const char *playlist_name, const PlayList *playlist);
void save_favorites_playlist(const char *directory, PlayList *favorites_playlist);
void deep_copy_play_list_onto_list(const PlayList *original_list, PlayList **new_list);
void create_play_list_from_file_system_entry(FileSystemEntry *root, PlayList *list, int playlist_max);
void add_shuffled_albums_to_play_list(FileSystemEntry *root, PlayList *list, int playlist_max);
void move_up_list(PlayList *list, Node *node);
void move_down_list(PlayList *list, Node *node);
Node *read_m3u_file(const char *filename, PlayList *playlist);
