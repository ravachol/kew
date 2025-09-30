#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef __USE_XOPEN_EXTENDED
#define __USE_XOPEN_EXTENDED
#endif

#include <pthread.h>
#include <stdbool.h>
#include "directorytree.h"

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

extern Node *currentSong;

#endif

extern PlayList playlist;
extern PlayList *unshuffledPlaylist;
extern PlayList *favoritesPlaylist;

extern int nodeIdCounter;

Node *getListNext(Node *node);

Node *getListPrev(Node *node);

void createNode(Node **node, const char *directoryPath, int id);

int addToList(PlayList *list, Node *newNode);

Node *deleteFromList(PlayList *list, Node *node);

void deletePlaylist(PlayList *playlist);

void shufflePlaylist(PlayList *playlist);

void shufflePlaylistStartingFromSong(PlayList *playlist, Node *song);

int makePlaylist(int argc, char *argv[], bool exactSearch, const char *path);

void writeCurrentPlaylistToM3UFile(PlayList *playlist);

void writeM3UFile(const char *filename, const PlayList *playlist);

void saveNamedPlaylist(const char *directory, const char *playlistName, const PlayList *playlist);

void exportCurrentPlaylist(const char *path);

void saveLastUsedPlaylist(void);

void loadLastUsedPlaylist(void);

PlayList deepCopyPlayList(PlayList *originalList);

void deepCopyPlayListOntoList(PlayList *originalList, PlayList *newList);

Node *findPathInPlaylist(const char *path, PlayList *playlist);

Node *findLastPathInPlaylist(const char *path, PlayList *playlist);

int findNodeInList(PlayList *list, int id, Node **foundNode);

void createPlayListFromFileSystemEntry(FileSystemEntry *root, PlayList *list, int playlistMax);

void addShuffledAlbumsToPlayList(FileSystemEntry *root, PlayList *list, int playlistMax);

void moveUpList(PlayList *list, Node *node);

void moveDownList(PlayList *list, Node *node);

void loadFavoritesPlaylist(const char *directory);

void saveFavoritesPlaylist(const char *directory);

int isMusicFile(const char *filename);

void readM3UFile(const char *filename, PlayList *playlist, FileSystemEntry *library);
