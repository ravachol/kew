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
#include <stdbool.h>

typedef struct
{
        gint width_cells, height_cells;
        gint width_pixels, height_pixels;
} TermSize;

void tty_init(void);
int print_in_ascii(int row, int col, const char *path_to_img_file, int height, bool centered);
int get_cover_color(unsigned char *pixels, int width, int height, unsigned char *r, unsigned char *g, unsigned char *b);
float get_aspect_ratio();
float calc_aspect_ratio(void);
unsigned char *get_bitmap(const char *image_path, int *width, int *height);
void print_square_bitmap(int row, int col, unsigned char *pixels, int width, int height, int base_height, bool centered);
void get_tty_size(TermSize *term_size_out);

#ifdef CHAFA_VERSION_1_16
gboolean retirePassthroughWorkarounds_tmux(void);
#endif

#endif
