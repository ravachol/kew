#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include "sound.h"

#ifndef PLAYLIST_STRUCT
#define PLAYLIST_STRUCT

typedef struct
{
    char *filePath;
    double duration;
} SongInfo;

typedef struct Node
{
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
} PlayList;

#endif

extern volatile int stopThread;

extern PlayList playlist;

extern PlayList* mainPlaylist;

Node *getListNext(Node *node);

Node *getListPrev(Node *node);

void addToList(PlayList *list, SongInfo song);

Node *deleteFromList(PlayList *list, Node *node);

int compare(const struct dirent **a, const struct dirent **b);

void buildPlaylistRecursive(char *directoryPath, const char *allowedExtensions, PlayList *playlist);

int playDirectory(const char *directoryPath, const char *allowedExtensions, PlayList *playlist);

void shufflePlaylist(PlayList *playlist);

int joinPlaylist(PlayList *dest, PlayList *src);

int makePlaylist(int argc, char *argv[]);

int calculatePlayListDuration(PlayList *playlist);

void readM3UFile(const char* filename, PlayList* playlist);

void writeM3UFile(const char* filename, PlayList* playlist);

void loadMainPlaylist(const char *directory);

void saveMainPlaylist(const char *directory);

Node* deepCopyNode(Node* originalNode);

PlayList deepCopyPlayList(PlayList* originalList);