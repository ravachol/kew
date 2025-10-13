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

#define MAX_FILES 10000

#ifndef PLAYLIST_STRUCT
#define PLAYLIST_STRUCT

typedef struct
{
        char *filePath;
        double duration;
} SongInfo;

typedef struct Node
{
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

int incrementNodeId(void);

void clearCurrentSong(void);

void setCurrentSong(Node *node);

Node *getCurrentSong(void);

Node *getListNext(Node *node);

Node *getListPrev(Node *node);

void createNode(Node **node, const char *directoryPath, int id);

int addToList(PlayList *list, Node *newNode);

Node *deleteFromList(PlayList *list, Node *node);

void deletePlaylist(PlayList *playlist);

void destroyNode(Node *node);

void shufflePlaylist(PlayList *playlist);

void shufflePlaylistStartingFromSong(PlayList *playlist, Node *song);

int makePlaylist(PlayList **playlist, int argc, char *argv[], bool exactSearch, const char *path);

void writeM3UFile(const char *filename, const PlayList *playlist);

void exportCurrentPlaylist(const char *path, const PlayList *playlist);

void loadLastUsedPlaylist(PlayList *playlist, PlayList **unshuffledPlaylist);

void loadFavoritesPlaylist(const char *directory, PlayList **favoritesPlaylist);

void saveNamedPlaylist(const char *directory, const char *playlistName, const PlayList *playlist);

void saveLastUsedPlaylist(PlayList *unshuffledPlaylist);

void saveFavoritesPlaylist(const char *directory, PlayList *favoritesPlaylist);

PlayList *deepCopyPlayList(const PlayList *originalList);

void deepCopyPlayListOntoList(const PlayList *originalList, PlayList **newList);

Node *findPathInPlaylist(const char *path, PlayList *playlist);

Node *findLastPathInPlaylist(const char *path, PlayList *playlist);

int findNodeInList(PlayList *list, int id, Node **foundNode);

void createPlayListFromFileSystemEntry(FileSystemEntry *root, PlayList *list, int playlistMax);

void addShuffledAlbumsToPlayList(FileSystemEntry *root, PlayList *list, int playlistMax);

void moveUpList(PlayList *list, Node *node);

void moveDownList(PlayList *list, Node *node);

int isMusicFile(const char *filename);

void readM3UFile(const char *filename, PlayList *playlist, FileSystemEntry *library);
