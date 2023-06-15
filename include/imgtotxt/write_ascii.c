/*
TODO:
    - Debug arguments parsing behaviour.
    - Options at beginning or end applied to all unless specified otherwise?

    - Improve 'scale' for plane ASCII output.
    - Preserve aspect-ratio option.

    - Check if 256 colors is default.
    - Check if rbg colors are widely supported.
    - Translating from rgb into 256 colors.

    - Windows support?
*/

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// Disable some warnings for stb headers.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wstrict-overflow"
#define STB_IMAGE_IMPLEMENTATION
#include "ext/stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "ext/stb_image_resize.h"
#pragma GCC diagnostic pop

#include "options.h"
#include "write_ascii.h"

typedef struct
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
} PixelData;

#define MAX_IMG_SIZE 25000

#define MACRO_STRLEN(s) (sizeof(s) / sizeof(s[0]))

char scale[] = "$@&B%8WM#ZO0QoahkbdpqwmLCJUYXIjft/\\|()1{}[]l?zcvunxr!<>i;:*-+~_,\"^`'. ";
unsigned int brightness_levels = MACRO_STRLEN(scale) - 2;

unsigned char luminanceFromRGB(unsigned char r, unsigned char g, unsigned char b)
{
    return (unsigned char)(0.2126 * r + 0.7152 * g + 0.0722 * b);
}

unsigned char calc_ascii_char(PixelData *p)
{
    // Calc luminace and use to find Ascii char.
    unsigned char ch = luminanceFromRGB(p->r, p->g, p->b);
    int rescaled = ch * brightness_levels / 256;
    return scale[brightness_levels - rescaled];
}

int read_and_convert(char *filepath, ImageOptions *options)
{
    int rwidth, rheight, rchannels;
    unsigned char *read_data = stbi_load(filepath, &rwidth, &rheight, &rchannels, 3);

    if (read_data == NULL)
    {
        fprintf(stderr, "Error reading image data!\n\n");
        return -1;
    }

    unsigned int desired_width, desired_height;
    desired_width = options->width;
    desired_height = options->height;

    // Check for and do any needed image resizing...
    PixelData *data;
    if (desired_width != (unsigned)rwidth || desired_height != (unsigned)rheight)
    {
        // 3 * uint8 for RGB!
        unsigned char *new_data = malloc(3 * sizeof(unsigned char) * desired_width * desired_height);
        int r = stbir_resize_uint8(
            read_data, rwidth, rheight, 0,
            new_data, desired_width, desired_height, 0, 3);

        if (r == 0)
        {
            perror("Error resizing image:");
            return -1;
        }
        stbi_image_free(read_data); // Free read_data.
        data = (PixelData *)new_data;
    }
    else
    {
        data = (PixelData *)read_data;
    }

    // Print header if required...
    if (options->suppress_header == false)
        printf(
            "Converting File: %s\n"
            "Img Type: %s\n"
            "True Color: %d\n"
            "Size: %dx%d\n",
            filepath,
            OutputModeStr[options->output_mode],
            options->true_color,
            desired_width,
            desired_height);

    if (!options->suppress_header)
        printf("\n");

    for (unsigned int d = 0; d < desired_width * desired_height; d++)
    {
        if (d % desired_width == 0 && d != 0)
        {
            if (options->output_mode == SOLID_ANSI)
                printf("\033[0m");
            printf("\n");
        }

        PixelData *c = data + d;

        switch (options->output_mode)
        {
        case ASCII:
            printf("%c", calc_ascii_char(c));
            break;
        case ANSI:
            printf("\033[1;38;2;%03u;%03u;%03um%c", c->r, c->g, c->b, calc_ascii_char(c));
            break;
        case SOLID_ANSI:
            printf("\033[48;2;%03u;%03u;%03um ", c->r, c->g, c->b);
            break;
        default:
            break;
        }
    }
    if (options->output_mode == SOLID_ANSI)
        printf("\033[0m");
    printf("\n");

    stbi_image_free(data);
    return 0;
}

int output_ascii(char *pathToImgFile, int height, int width, bool coverBlocks)
{
    ImageOptions opts = {
        .output_mode = ANSI,
        .original_size = false,
        .true_color = true,
        .squashing_enabled = true,
        .suppress_header = true,
    };

    if (coverBlocks)
        opts.output_mode = SOLID_ANSI;

    if (width > MAX_IMG_SIZE)
    {
        fprintf(stderr, "[ERR] Width exceeds maximum image size!\n");
        return -1;
    }
    opts.width = width;

    if (height > MAX_IMG_SIZE)
    {
        fprintf(stderr, "[ERR] Height exceeds maximum image size!\n");
        return -1;
    }
    opts.height = height;

    int ret = read_and_convert(pathToImgFile, &opts);
    if (ret == -1)
        //  fprintf(stderr, "Failed to convert image: %s\n", pathToImgFile);
        printf("\033[0m\n");
    return 0;
}