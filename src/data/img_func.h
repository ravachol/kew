/**
 * @file img_func.h
 * @brief Image rendering and conversion helpers using Chafa.
 *
 * Handles loading and displaying album art or other images in the terminal
 * using the Chafa library. Supports scaling, aspect-ratio preservation,
 * and different rendering modes (truecolor, ASCII, etc.).
 */

#ifndef IMG_FUNC_H
#define IMG_FUNC_H

#include <chafa.h>

int print_in_ascii_centered(const char *path_to_img_file, int height);
int print_in_ascii(int indentation, const char *path_to_img_file, int height);
int get_cover_color(unsigned char *pixels, int width, int height, unsigned char *r, unsigned char *g, unsigned char *b);
float get_aspect_ratio();
float calc_aspect_ratio(void);
unsigned char *get_bitmap(const char *image_path, int *width, int *height);
void print_square_bitmap_centered(unsigned char *pixels, int width, int height, int base_height);
void print_square_bitmap(int row, int col, unsigned char *pixels, int width, int height, int base_height);

#ifdef CHAFA_VERSION_1_16
gboolean retirePassthroughWorkarounds_tmux(void);
#endif

#endif
