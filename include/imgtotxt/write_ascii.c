/*
    Modified, originally by Danny Burrows:
    https://github.com/danny-burrows/img_to_txt
*/

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "../../src/term.h"

// Disable some warnings for stb headers.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wstrict-overflow"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>
#pragma GCC diagnostic pop
#include "write_ascii.h"

#define MACRO_STRLEN(s) (sizeof(s) / sizeof(s[0]))

char scale[] = "$@&B%8WM#ZO0QoahkbdpqwmLCJUYXIjft/\\|()1{}[]l?zcvunxr!<>i;:*-+~_,\"^`'. ";
unsigned int brightness_levels = MACRO_STRLEN(scale) - 2;

unsigned char luminanceFromRGB(unsigned char r, unsigned char g, unsigned char b)
{
        return (unsigned char)(0.2126 * r + 0.7152 * g + 0.0722 * b);
}

unsigned char calcAsciiChar(PixelData *p)
{
        unsigned char ch = luminanceFromRGB(p->r, p->g, p->b);
        int rescaled = ch * brightness_levels / 256;

        return scale[brightness_levels - rescaled];
}

int convertToAscii(const char *filepath, unsigned int width, unsigned int height)
{
        int rwidth, rheight, rchannels;
        unsigned char *read_data = stbi_load(filepath, &rwidth, &rheight, &rchannels, 3);

        if (read_data == NULL)
        {
                return -1;
        }

        PixelData *data;
        if (width != (unsigned)rwidth || height != (unsigned)rheight)
        {
                // 3 * uint8 for RGB!
                unsigned char *new_data = malloc(3 * sizeof(unsigned char) * width * height);
                stbir_resize_uint8_srgb(
                    read_data, rwidth, rheight, 0,
                    new_data, width, height, 0, 3);

                stbi_image_free(read_data);
                data = (PixelData *)new_data;
        }
        else
        {
                data = (PixelData *)read_data;
        }

        int term_w, term_h;
        getTermSize(&term_w, &term_h);

        int indent = ((term_w - width) / 2) + 1;

        printf("\n");
        printf("%*s", indent, "");

        for (unsigned int d = 0; d < width * height; d++)
        {
                if (d % width == 0 && d != 0)
                {
                        printf("\n");
                        printf("%*s", indent, "");
                }

                PixelData *c = data + d;

                printf("\033[1;38;2;%03u;%03u;%03um%c", c->r, c->g, c->b, calcAsciiChar(c));
        }

        printf("\n");

        stbi_image_free(data);
        return 0;
}

int printInAscii(const char *pathToImgFile, int height, int width)
{
        width -= 1;

        printf("\r");

        int ret = convertToAscii(pathToImgFile, (unsigned)width, (unsigned)height);
        if (ret == -1)
                printf("\033[0m");
        return 0;
}
