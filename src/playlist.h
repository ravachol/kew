#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "sound.h"

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
} PlayList;

extern PlayList playlist;

Node *getListNext(PlayList *list, Node *node);

Node *getListPrev(PlayList *list, Node *node);

void addToList(PlayList *list, SongInfo song);

int compare(const struct dirent **a, const struct dirent **b);

void buildPlaylistRecursive(char *directoryPath, const char *allowedExtensions, PlayList *playlist);

int playDirectory(const char *directoryPath, const char *allowedExtensions, PlayList *playlist);

void shufflePlaylist(PlayList *playlist);

int joinPlaylist(PlayList *dest, PlayList *src);

int makePlaylist(int argc, char *argv[]);