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

/**
 * Initializes TTY-specific settings.
 *
 * On Windows, enables ANSI escape sequence processing and
 * configures the console to use UTF-8 for input and output.
 * On other platforms, this function currently performs no action.
 */
void tty_init(void);

/**
 * Prints an image file as colored ASCII art.
 *
 * Loads the image from disk, rescales it according to the
 * terminal cell aspect ratio, and renders it using ANSI
 * escape sequences.
 *
 * @param row              The starting row in the terminal
 * @param col              The starting column in the terminal
 * @param path_to_img_file Path to the image file
 * @param height           Target height in terminal cells
 * @param centered         Whether the output should be horizontally centered
 *
 * @return Always returns 0 (prints reset sequence on failure)
 */
int print_in_ascii(int row, int col,
                   const char *path_to_img_file,
                   int height,
                   bool centered);

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
                    unsigned char *r,
                    unsigned char *g,
                    unsigned char *b);

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

/**
 * Prints a square bitmap image in the terminal using Chafa.
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
 */
void print_square_bitmap(int row,
                         int col,
                         unsigned char *pixels,
                         int width,
                         int height,
                         int base_height,
                         bool centered);

/**
 * Retrieves the current terminal size.
 *
 * Fills the provided TermSize structure with terminal
 * dimensions in both character cells and pixels
 * (if available). Unsupported values are set to -1.
 *
 * @param term_size_out Output parameter receiving terminal size data
 */
void get_tty_size(TermSize *term_size_out);

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

#endif
