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

#include "common/model.h"

#include <chafa.h>
#include <stdbool.h>

/**
 * Extracts a representative bright color from an RGBA bitmap.
 *
 * Iterates through the pixel buffer and selects the first
 * sufficiently bright and non-gray pixel.
 *
 * @param pixels Pointer to RGBA pixel buffer
 * @param width  Image width in pixels
 * @param height Image height in pixels
 * @param r      Output parameter for red component
 * @param g      Output parameter for green component
 * @param b      Output parameter for blue component
 *
 * @return 0 if a suitable color was found, -1 otherwise
 */
int get_cover_color(unsigned char *pixels,
                    int width,
                    int height,
                    int *r,
                    int *g,
                    int *b);

/**
 * Draws a square bitmap image to a buffer using Chafa.
 *
 * Converts the RGBA pixel buffer into a printable terminal
 * representation and renders it at the specified position.
 * The width is adjusted to compensate for terminal cell
 * aspect ratio.
 *
 * @param row         Starting row in the terminal
 * @param col         Starting column in the terminal
 * @param pixels      RGBA pixel buffer
 * @param width       Image width in pixels
 * @param height      Image height in pixels
 * @param base_height Target height in terminal cells
 * @param centered    Whether the output should be horizontally centered
 * @param just_mark_cover  Whether to just mark the x, y of the cover with an empty image
 * @param draw_occupied_markers Whether to mark cells as occupied
 */
void draw_square_bitmap_to_buf(DrawBuffer *buf, int row, int col,
                               unsigned char *pixels, int width, int height, int max_width,
                               int base_height, const TermSize *term_size, bool centered, size_t img_hash,
                               const char *cover_style, int just_mark_cover, bool draw_occupied_markers);

/**
 * Returns the terminal cell aspect ratio.
 *
 * Calculates the ratio of cell height to cell width,
 * using terminal pixel and cell dimensions if available.
 * Falls back to a default of 8x16 cell size when necessary.
 *
 * @return Aspect ratio (cell_height / cell_width)
 */
float get_aspect_ratio(void);

/**
 * Calculates the terminal cell aspect ratio.
 *
 * Queries the terminal dimensions and computes the
 * height-to-width ratio of a single character cell.
 * Uses default dimensions if pixel metrics are unavailable.
 *
 * @return Aspect ratio (cell_height / cell_width)
 */
float calc_aspect_ratio(void);

/**
 * Loads an image file into an RGBA bitmap buffer.
 *
 * Uses stb_image to decode the image and forces
 * 4 channels (RGBA).
 *
 * @param image_path Path to the image file
 * @param width      Output parameter for image width
 * @param height     Output parameter for image height
 *
 * @return Pointer to allocated RGBA pixel buffer,
 *         or NULL on failure. Caller must free with stbi_image_free().
 */
unsigned char *get_bitmap(const char *image_path,
                          int *width,
                          int *height);

#ifdef CHAFA_VERSION_1_16
/**
 * Restores tmux passthrough settings modified for image rendering.
 *
 * If passthrough mode was temporarily changed to enable pixel
 * passthrough support, this function restores the original
 * tmux configuration.
 *
 * @return TRUE on success or if no restoration was required,
 *         FALSE on failure
 */
gboolean retirePassthroughWorkarounds_tmux(void);

#endif

void free_image_payload(ImagePayload *img);

#endif
