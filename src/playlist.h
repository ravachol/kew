#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef __USE_XOPEN_EXTENDED
#define __USE_XOPEN_EXTENDED
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include "songloader.h"

#ifndef PLAYLIST_STRUCT
#define PLAYLIST_STRUCT

#define MAX_COUNT_PLAYLIST_SONGS 200

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
        volatile double totalDuration;
        pthread_mutex_t mutex;        
} PlayList;

extern Node *currentSong;

#endif

extern PlayList playlist;

extern PlayList *mainPlaylist;
extern PlayList *originalPlaylist;
extern int nodeIdCounter;

Node *getListNext(Node *node);

Node *getListPrev(Node *node);

void createNode(Node **node, char *directoryPath, int id);

void addToList(PlayList *list, Node *newNode);

double calcTotalDuration(PlayList *playList);

Node *deleteFromList(PlayList *list, Node *node);

void byPassNode(PlayList *list, Node *node);

void deletePlaylist(PlayList *playlist);

int compare(const struct dirent **a, const struct dirent **b);

void buildPlaylistRecursive(char *directoryPath, const char *allowedExtensions, PlayList *playlist);

int playDirectory(const char *directoryPath, const char *allowedExtensions, PlayList *playlist);

void shufflePlaylist(PlayList *playlist);

void shufflePlaylistStartingFromSong(PlayList *playlist, Node *song);

int joinPlaylist(PlayList *dest, PlayList *src);

int makePlaylist(int argc, char *argv[], bool exactSearch);

int calculatePlayListDuration(PlayList *playlist);

void stopPlayListDurationCount(void);

void startPlayListDurationCount(void);

void readM3UFile(const char *filename, PlayList *playlist);

void writeM3UFile(const char *filename, PlayList *playlist);

void loadMainPlaylist(const char *directory);

void saveMainPlaylist(const char *directory, bool isPlayingMain);

void savePlaylist(void);

Node *deepCopyNode(Node *originalNode);

PlayList deepCopyPlayList(PlayList *originalList);

Node *findSongInPlaylist(Node *currentSong, PlayList *playlist);

Node *findPathInPlaylist(char *path, PlayList *playlist);

Node *findLastPathInPlaylist(char *path, PlayList *playlist);

int findNodeInList(PlayList *list, int id, Node **foundNode);
