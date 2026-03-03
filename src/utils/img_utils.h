

#ifndef IMG_UTILS_H
#define IMG_UTILS_H

/**
 * @brief Represents an RGB pixel color.
 *
 * Stores red, green, and blue components as 8-bit unsigned values.
 */
typedef struct
{
        unsigned char r; /**< Red component (0–255). */
        unsigned char g; /**< Green component (0–255). */
        unsigned char b; /**< Blue component (0–255). */
} PixelData;

unsigned char *get_bitmap(const char *image_path, int *width, int *height);

// Loads an image in 8-bit RGB format
// Returns pointer to pixel data (must be freed with image_free)
unsigned char *image_load_rgb(const char *filepath, int *width, int *height, int *channels);

unsigned char *image_resize_uint8_srgb(
    const unsigned char *src,
    int src_w,
    int src_h,
    unsigned char *dst,
    int dst_w,
    int dst_h,
    int channels);

void image_free(void *data);

int get_cover_color(unsigned char *pixels, int width, int height, int *r, int *g, int *b);

unsigned char calc_ascii_char(PixelData *p);

#endif
