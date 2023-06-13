#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct 
{
    char* filePath;    
} SongInfo;

typedef struct Node 
{
    SongInfo song;
    struct Node* next;
    struct Node* prev;
} Node;

typedef struct {
    Node* head;
    Node* tail;
    int count;
} PlayList;

Node* getListNext(PlayList* list, Node* node);

Node* getListPrev(PlayList* list, Node* node);

void addToList(PlayList* list, SongInfo song);

int compare(const struct dirent **a, const struct dirent **b);

void buildPlaylistRecursive(char* directoryPath, const char* allowedExtensions, PlayList* playlist, bool shuffle);

int playDirectory(const char* directoryPath, const char* allowedExtensions, PlayList* playlist);