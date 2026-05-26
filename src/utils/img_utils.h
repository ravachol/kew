

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
        int a; /**< Alpha component (0–255). */
} PixelData;

#define COLOR_DEFAULT      ((PixelData){ .r = 0, .g = 0, .b = 0, .a = -1 })
#define COLOR_RGB(r, g, b) ((PixelData){ .r = r, .g = g, .b = b, .a = 255 })
#define COLOR_RGBA(r,g,b,a)((PixelData){ .r = r, .g = g, .b = b, .a = a   })
#define COLOR_IS_DEFAULT(c) ((c).a == -1)

/**
 * @brief Loads a bitmap image and returns its raw pixel data.
 *
 * @param image_path Path to the image file.
 * @param width      Output parameter set to the image width in pixels.
 * @param height     Output parameter set to the image height in pixels.
 * @return Pointer to the raw pixel data, or NULL on failure. Must be freed with image_free().
 */
unsigned char *get_bitmap(const char *image_path, int *width, int *height);

/**
 * @brief Loads an image and decodes it into 8-bit RGB format.
 *
 * @param filepath Path to the image file.
 * @param width    Output parameter set to the image width in pixels.
 * @param height   Output parameter set to the image height in pixels.
 * @param channels Output parameter set to the number of color channels in the source image.
 * @return Pointer to the decoded pixel data, or NULL on failure. Must be freed with image_free().
 */
unsigned char *image_load_rgb(const char *filepath, int *width, int *height, int *channels);

/**
 * @brief Resizes an 8-bit sRGB image into a caller-provided destination buffer.
 *
 * @param src      Pointer to the source pixel data.
 * @param src_w    Width of the source image in pixels.
 * @param src_h    Height of the source image in pixels.
 * @param dst      Pointer to the destination buffer to write the resized image into.
 * @param dst_w    Width of the destination image in pixels.
 * @param dst_h    Height of the destination image in pixels.
 * @param channels Number of color channels (e.g. 3 for RGB, 4 for RGBA).
 * @return Pointer to the destination buffer, or NULL on failure.
 */
unsigned char *image_resize_uint8_srgb(
    const unsigned char *src,
    int src_w,
    int src_h,
    unsigned char *dst,
    int dst_w,
    int dst_h,
    int channels);

/**
 * @brief Frees pixel data allocated by image loading functions.
 *
 * @param data Pointer to the pixel data to free.
 */
void image_free(void *data);

/**
 * @brief Computes the dominant cover color of an image.
 *
 * @param pixels Pointer to the raw RGB pixel data.
 * @param width  Width of the image in pixels.
 * @param height Height of the image in pixels.
 * @param r      Output parameter set to the red component of the dominant color.
 * @param g      Output parameter set to the green component of the dominant color.
 * @param b      Output parameter set to the blue component of the dominant color.
 * @return 0 on success, or a non-zero error code on failure.
 */
int get_cover_color(unsigned char *pixels, int width, int height, int *r, int *g, int *b);

/**
 * @brief Maps a pixel's color to a corresponding ASCII character.
 *
 * Typically used for ASCII art rendering, where brighter pixels map
 * to denser characters and darker pixels to sparser ones.
 *
 * @param p Pointer to the PixelData to evaluate.
 * @return An ASCII character representing the pixel's brightness.
 */
unsigned char calc_ascii_char(PixelData *p);

#endif
