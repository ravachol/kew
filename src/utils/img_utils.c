// Disable some warnings for stb headers.
#include "common/appstate.h"
#include "common/model.h"
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

bool check_if_bright_pixel(unsigned char r, unsigned char g, unsigned char b)
{
        // Calc luminace and use to find Ascii char.
        unsigned char ch = luminance_from_r_g_b(r, g, b);

        if (ch > 80 && !(r < g + 20 && r > g - 20 && g < b + 20 && g > b - 20) && !(r > 150 && g > 150 && b > 150))
                return true;

        return false;
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

                if (check_if_bright_pixel(red, green, blue)) {
                        *r = (int)red;
                        *g = (int)green;
                        *b = (int)blue;
                        break;
                }
        }

        return found ? 0 : -1;
}

static void run_kmeans(PixelData *samples, int num_samples, PixelData centroids[3])
{
        if (num_samples < 3) {
                for (int k = 0; k < 3; k++) {
                        centroids[k] = (num_samples > 0) ? samples[0] : (PixelData){128, 128, 128, 255};
                }
                return;
        }

        centroids[0] = samples[0];
        centroids[1] = samples[num_samples / 2];
        centroids[2] = samples[num_samples - 1];

        int assignments[1024];
        if (num_samples > 1024)
                num_samples = 1024;

        long long sum_r[3], sum_g[3], sum_b[3];
        int count[3];

        for (int iter = 0; iter < 6; iter++) {
                for (int i = 0; i < num_samples; i++) {
                        double min_dist = 1e9;
                        int best_k = 0;
                        for (int k = 0; k < 3; k++) {
                                double dr = samples[i].r - centroids[k].r;
                                double dg = samples[i].g - centroids[k].g;
                                double db = samples[i].b - centroids[k].b;
                                double dist = dr * dr + dg * dg + db * db;
                                if (dist < min_dist) {
                                        min_dist = dist;
                                        best_k = k;
                                }
                        }
                        assignments[i] = best_k;
                }

                for (int k = 0; k < 3; k++) {
                        sum_r[k] = sum_g[k] = sum_b[k] = 0;
                        count[k] = 0;
                }
                for (int i = 0; i < num_samples; i++) {
                        int k = assignments[i];
                        sum_r[k] += samples[i].r;
                        sum_g[k] += samples[i].g;
                        sum_b[k] += samples[i].b;
                        count[k]++;
                }
                for (int k = 0; k < 3; k++) {
                        if (count[k] > 0) {
                                centroids[k].r = sum_r[k] / count[k];
                                centroids[k].g = sum_g[k] / count[k];
                                centroids[k].b = sum_b[k] / count[k];
                        } else {
                                centroids[k] = samples[rand() % num_samples];
                        }
                }
        }

        for (int i = 0; i < 2; i++) {
                for (int j = i + 1; j < 3; j++) {
                        if (count[j] > count[i]) {
                                int tmp_c = count[i];
                                count[i] = count[j];
                                count[j] = tmp_c;
                                PixelData tmp_p = centroids[i];
                                centroids[i] = centroids[j];
                                centroids[j] = tmp_p;
                        }
                }
        }
}

void load_kmeans_palette(unsigned char *pixels, int width, int height, PixelData kmeans_palette[3])
{
        if (pixels == NULL || width <= 0 || height <= 0) {
                return;
        }

        int channels = 4;
        int step_y = height / 16;
        int step_x = width / 16;

        if (step_y < 1)
                step_y = 1;
        if (step_x < 1)
                step_x = 1;

        // Collect samples for K-Means and Top-N Vibrant
        PixelData samples[1024];
        int num_samples = 0;
        for (int y = 0; y < height && num_samples < 1024; y += step_y) {
                for (int x = 0; x < width && num_samples < 1024; x += step_x) {
                        int index = (y * width + x) * channels;
                        unsigned char r = pixels[index];
                        unsigned char g = pixels[index + 1];
                        unsigned char b = pixels[index + 2];

                        if (check_if_bright_pixel(r, g, b)) {
                                samples[num_samples++] = (PixelData){r, g, b, 255};
                        }
                }
        }

        if (num_samples == 0) {
                for (int y = 0; y < height && num_samples < 1024; y += step_y) {
                        for (int x = 0; x < width && num_samples < 1024; x += step_x) {
                                int index = (y * width + x) * channels;
                                samples[num_samples++] = (PixelData){pixels[index], pixels[index + 1], pixels[index + 2], 255};
                        }
                }
        }

        run_kmeans(samples, num_samples, kmeans_palette);

        Model *model = get_model();
        for (int i = 0; i < 3; i++)
        {
                if (!check_if_bright_pixel(kmeans_palette[i].r, kmeans_palette[i].g, kmeans_palette[i].b))
                        kmeans_palette[i] = model->state.settings.defaultColorRGB;
        }
}
