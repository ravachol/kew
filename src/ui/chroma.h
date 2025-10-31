#ifndef CHROMA_H
#define CHROMA_H

#include <pthread.h>
#include <stdbool.h>


#define BYTES_PER_CELL 32
#define MAX_HEIGHT 4000

extern volatile int chroma_new_frame;

typedef struct {
    char *frame;
    pthread_mutex_t lock;
    bool running;
    pthread_t thread;
    size_t frame_capacity;
} Chroma;

extern Chroma g_viz;

void chroma_start(int height);

void chroma_stop(void);

const char *chroma_get_frame(void);

bool chroma_is_installed(void);

void print_chroma_frame(int row, int col);

#endif
