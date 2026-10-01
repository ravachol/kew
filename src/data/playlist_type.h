#ifndef PLAYLIST_STRUCT
#define PLAYLIST_STRUCT

#include <pthread.h>
#include <stdint.h>

typedef struct
{
        char *file_path;
        double duration;
} SongInfo;

typedef struct Node {
        int id;
        uint64_t tracklist_id;
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
