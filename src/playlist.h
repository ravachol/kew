#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include "soundgapless.h"

#ifndef PLAYLIST_STRUCT
#define PLAYLIST_STRUCT

#define MAX_COUNT_PLAYLIST_SONGS 100

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

extern Node *currentSong;

#endif

extern PlayList playlist;

extern PlayList *mainPlaylist;

Node *getListNext(Node *node);

Node *getListPrev(Node *node);

void addToList(PlayList *list, SongInfo song);

Node *deleteFromList(PlayList *list, Node *node);

void deletePlaylist(PlayList *playlist);

int compare(const struct dirent **a, const struct dirent **b);

void buildPlaylistRecursive(char *directoryPath, const char *allowedExtensions, PlayList *playlist);

int playDirectory(const char *directoryPath, const char *allowedExtensions, PlayList *playlist);

void shufflePlaylist(PlayList *playlist);

void shufflePlaylistStartingFromSong(PlayList *playlist, Node *song);

int joinPlaylist(PlayList *dest, PlayList *src);

int makePlaylist(int argc, char *argv[]);

int calculatePlayListDuration(PlayList *playlist);

void stopPlayListDurationCount();

void startPlayListDurationCount();

void readM3UFile(const char *filename, PlayList *playlist);

void writeM3UFile(const char *filename, PlayList *playlist);

void loadMainPlaylist(const char *directory);

void saveMainPlaylist(const char *directory, bool isPlayingMain);

void savePlaylist();

Node *deepCopyNode(Node *originalNode);

PlayList deepCopyPlayList(PlayList *originalList);
