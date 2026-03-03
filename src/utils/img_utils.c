// Disable some warnings for stb headers.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wstrict-overflow"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>
#pragma GCC diagnostic pop

#include "img_utils.h"

#include <stdbool.h>

#define MACRO_STRLEN(s) (sizeof(s) / sizeof(s[0]))

char scale[] = "$@&B%8WM#ZO0QoahkbdpqwmLCJUYXIjft/\\|()1{}[]l?zcvunxr!<>i;:*-+~_,\"^`'. ";
unsigned int brightness_levels = MACRO_STRLEN(scale) - 2;

// The function to load and return image data
unsigned char *get_bitmap(const char *image_path, int *width, int *height)
{
        if (image_path == NULL)
                return NULL;

        int channels;

        unsigned char *image = stbi_load(image_path, width, height, &channels, 4); // Force 4 channels (RGBA)
        if (!image) {
                fprintf(stderr, "Failed to load image: %s\n", image_path);
                return NULL;
        }

        return image;
}

unsigned char *image_load_rgb(const char *filepath, int *width, int *height, int *channels)
{
        // Force 3 channels (RGB)
        return stbi_load(filepath, width, height, channels, 3);
}

unsigned char *image_resize_uint8_srgb(
    const unsigned char *src,
    int src_w,
    int src_h,
    unsigned char *dst,
    int dst_w,
    int dst_h,
    int channels)
{
        return stbir_resize_uint8_srgb(
            src, src_w, src_h, 0,
            dst, dst_w, dst_h, 0,
            channels);
}

void image_free(void *data)
{
        stbi_image_free(data);
}

unsigned char luminance_from_r_g_b(unsigned char r, unsigned char g, unsigned char b)
{
        return (unsigned char)(0.2126 * r + 0.7152 * g + 0.0722 * b);
}

void check_if_bright_pixel(unsigned char r, unsigned char g, unsigned char b, bool *found)
{
        // Calc luminace and use to find Ascii char.
        unsigned char ch = luminance_from_r_g_b(r, g, b);

        if (ch > 80 && !(r < g + 20 && r > g - 20 && g < b + 20 && g > b - 20) && !(r > 150 && g > 150 && b > 150)) {
                *found = true;
        }
}

unsigned char calc_ascii_char(PixelData *p)
{
        unsigned char ch = luminance_from_r_g_b(p->r, p->g, p->b);
        int rescaled = ch * brightness_levels / 256;

        return scale[brightness_levels - rescaled];
}

int get_cover_color(unsigned char *pixels, int width, int height, int *r, int *g, int *b)
{
        if (pixels == NULL || width <= 0 || height <= 0) {
                return -1;
        }

        int channels = 4; // RGBA format

        bool found = false;
        int num_pixels = width * height;

        for (int i = 0; i < num_pixels; i++) {
                int index = i * channels;
                unsigned char red = pixels[index + 0];
                unsigned char green = pixels[index + 1];
                unsigned char blue = pixels[index + 2];

                check_if_bright_pixel(red, green, blue, &found);

                if (found) {
                        *r = (int)red;
                        *g = (int)green;
                        *b = (int)blue;
                        break;
                }
        }

        return found ? 0 : -1;
}
