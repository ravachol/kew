#ifndef CHROMA_H
#define CHROMA_H

#include <pthread.h>
#include <stdbool.h>


#define BYTES_PER_CELL 32

#define MAX_HEIGHT 4000

#define MAX_PRESET 22

void chroma_start(int height);

void chroma_stop(void);

const char *chroma_get_frame(void);

bool chroma_is_installed(void);

void print_chroma_frame(int row, int col, bool centered);

bool is_chroma_started(void);

void chroma_set_next_preset(int height);

#endif
