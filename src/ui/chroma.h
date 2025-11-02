/**
 * @file chroma.h
 * @brief To handle running Chroma in a thread and printing its output
 *
 * Contains functions to start, stop and print a chroma frame.
 */

#ifndef CHROMA_H
#define CHROMA_H

#include <stdbool.h>

void chroma_start(int height);
void chroma_stop(void);
void chroma_print_frame(int row, int col, bool centered);
void chroma_set_next_preset(int height);
bool chroma_is_installed(void);
bool chroma_is_started(void);

#endif
