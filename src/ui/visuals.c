/**
 * @file visuals.c
 * @brief Audio visualization rendering.
 *
 * Implements a spectrum visualizer that react
 * to playback data.
 */

#include "common/appstate.h"

#include "common_ui.h"

#include "visuals.h"

#include "common/appstate.h"

#include "sound/sound_facade.h"

#include <complex.h>
#include <fftw3.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/model.h"
#include "loader/songdatatype.h"
#include "utils/img_utils.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_BARS 26 // Counting 1/3 octave per bar, 50hz-10000hz range

float *blackman_harris_window;

static void generate_legacy_palette(ColorPalette *palette, PixelData base, int height, int mode)
{
        int count = height < 16 ? height : 16;
        palette->count = count;
        for (int j = 1; j <= count; j++) {
                if (mode == 0)
                        palette->colors[j - 1] = increase_luminosity_for_height(base, height, j, false);
                else
                        palette->colors[j - 1] = increase_luminosity_for_height(base, height, j, true);
        }
}

static void generate_kmeans_palette(ColorPalette *palette, const unsigned char *pixels, int w, int h, int channels)
{
        int total_pixels = w * h;
        if (total_pixels <= 0 || pixels == NULL) {
                palette->count = 0;
                return;
        }

        int bucket_best_r[8], bucket_best_g[8], bucket_best_b[8];
        int bucket_best_vib[8];
        for (int i = 0; i < 8; i++) {
                bucket_best_r[i] = 0;
                bucket_best_g[i] = 0;
                bucket_best_b[i] = 0;
                bucket_best_vib[i] = -1;
        }

        int step = total_pixels < 1600 ? 1 : total_pixels / 1600;
        if (step < 1)
                step = 1;

        for (int s = 0; s < total_pixels; s += step) {
                int idx = s * channels;
                int r = pixels[idx];
                int g = pixels[idx + 1];
                int b = pixels[idx + 2];

                int lum = (54 * r + 183 * g + 19 * b) >> 8;
                int bucket = lum >> 5;
                if (bucket > 7)
                        bucket = 7;

                int maxc = r;
                if (g > maxc)
                        maxc = g;
                if (b > maxc)
                        maxc = b;
                int minc = r;
                if (g < minc)
                        minc = g;
                if (b < minc)
                        minc = b;
                int sat = maxc - minc;
                int brightness = maxc;
                int vib = sat * 3 + brightness;

                if (vib > bucket_best_vib[bucket]) {
                        bucket_best_vib[bucket] = vib;
                        bucket_best_r[bucket] = r;
                        bucket_best_g[bucket] = g;
                        bucket_best_b[bucket] = b;
                }
        }

        int count = 0;
        for (int i = 0; i < 8; i++) {
                if (bucket_best_vib[i] >= 0) {
                        palette->colors[count++] = (PixelData){
                            .r = (unsigned char)bucket_best_r[i],
                            .g = (unsigned char)bucket_best_g[i],
                            .b = (unsigned char)bucket_best_b[i],
                            .a = 255};
                }
        }
        palette->count = count;
}

#define BIN_SHIFT 5
#define BIN_SIZE (256 >> BIN_SHIFT)
#define BIN_TOP 12

static void generate_binning_palette(ColorPalette *palette, const unsigned char *pixels, int w, int h, int channels)
{
        int total_pixels = w * h;
        if (total_pixels <= 0 || pixels == NULL) {
                palette->count = 0;
                return;
        }

        int bin_counts[BIN_SIZE][BIN_SIZE][BIN_SIZE];
        memset(bin_counts, 0, sizeof(bin_counts));

        for (int i = 0; i < total_pixels; i++) {
                int idx = i * channels;
                int br = pixels[idx] >> BIN_SHIFT;
                int bg = pixels[idx + 1] >> BIN_SHIFT;
                int bb = pixels[idx + 2] >> BIN_SHIFT;
                int max_bin = BIN_SIZE - 1;
                if (br > max_bin)
                        br = max_bin;
                if (bg > max_bin)
                        bg = max_bin;
                if (bb > max_bin)
                        bb = max_bin;
                bin_counts[br][bg][bb]++;
        }

        typedef struct {
                int count, r_idx, g_idx, b_idx;
        } BinEntry;
        BinEntry top[BIN_TOP];
        int top_count = 0;

        for (int r = 0; r < BIN_SIZE; r++) {
                for (int g = 0; g < BIN_SIZE; g++) {
                        for (int b = 0; b < BIN_SIZE; b++) {
                                int cnt = bin_counts[r][g][b];
                                if (cnt == 0)
                                        continue;
                                int pos = top_count < BIN_TOP ? top_count : BIN_TOP - 1;
                                for (int p = 0; p < top_count && p < BIN_TOP; p++) {
                                        if (cnt > top[p].count) {
                                                pos = p;
                                                break;
                                        }
                                }
                                if (top_count < BIN_TOP)
                                        top_count++;
                                for (int p = top_count - 1; p > pos; p--)
                                        top[p] = top[p - 1];
                                top[pos].count = cnt;
                                top[pos].r_idx = r;
                                top[pos].g_idx = g;
                                top[pos].b_idx = b;
                        }
                }
        }

        palette->count = top_count;
        for (int i = 0; i < top_count; i++) {
                palette->colors[i] = (PixelData){
                    .r = (unsigned char)((top[i].r_idx << BIN_SHIFT) | (BIN_SHIFT > 0 ? (1 << (BIN_SHIFT - 1)) : 0)),
                    .g = (unsigned char)((top[i].g_idx << BIN_SHIFT) | (BIN_SHIFT > 0 ? (1 << (BIN_SHIFT - 1)) : 0)),
                    .b = (unsigned char)((top[i].b_idx << BIN_SHIFT) | (BIN_SHIFT > 0 ? (1 << (BIN_SHIFT - 1)) : 0)),
                    .a = 255};
        }
}

#define TOPN_VIBRANT_N 8
#define TOPN_SAMPLES 600
#define TOPN_LUM_THRESHOLD 80
#define TOPN_MIN_DIST_SQ 3600.0f
#define TOPN_HALF (TOPN_VIBRANT_N / 2)

typedef struct {
        float score;
        unsigned char r, g, b;
} VibEntry;

static void init_vib_entries(VibEntry *entries, int n)
{
        for (int i = 0; i < n; i++)
                entries[i].score = -1.0f;
}

static int insert_vib_entry(VibEntry *entries, int count, int max, float vibrance, unsigned char r, unsigned char g, unsigned char b)
{
        int pos = -1;
        for (int p = 0; p < count && p < max; p++) {
                if (vibrance > entries[p].score) {
                        pos = p;
                        break;
                }
        }
        if (pos == -1 && count < max)
                pos = count;

        if (pos < 0)
                return count;

        for (int p = 0; p < count; p++) {
                if (p == pos)
                        continue;
                float dr = (float)r - entries[p].r;
                float dg = (float)g - entries[p].g;
                float db = (float)b - entries[p].b;
                if (dr * dr + dg * dg + db * db < TOPN_MIN_DIST_SQ)
                        return count;
        }

        for (int p = max - 1; p > pos; p--)
                entries[p] = entries[p - 1];
        entries[pos].score = vibrance;
        entries[pos].r = r;
        entries[pos].g = g;
        entries[pos].b = b;
        if (count < max)
                count++;
        return count;
}

static void generate_topn_vibrant_palette(ColorPalette *palette, const unsigned char *pixels, int w, int h, int channels)
{
        int total_pixels = w * h;
        if (total_pixels <= 0 || pixels == NULL) {
                palette->count = 0;
                return;
        }

        VibEntry dark[TOPN_HALF];
        VibEntry bright[TOPN_HALF];
        init_vib_entries(dark, TOPN_HALF);
        init_vib_entries(bright, TOPN_HALF);
        int dark_count = 0;
        int bright_count = 0;

        int step = total_pixels / TOPN_SAMPLES;
        if (step < 1)
                step = 1;

        for (int i = 0; i < total_pixels; i += step) {
                int idx = i * channels;
                unsigned char cr = pixels[idx];
                unsigned char cg = pixels[idx + 1];
                unsigned char cb = pixels[idx + 2];
                float r = (float)cr;
                float g = (float)cg;
                float b = (float)cb;

                float max_c = r > g ? r : g;
                if (b > max_c)
                        max_c = b;
                float min_c = r < g ? r : g;
                if (b < min_c)
                        min_c = b;
                float chroma = max_c - min_c;
                float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                float vibrance = chroma * (1.0f - fabsf(lum - 128.0f) / 128.0f);

                if (lum < TOPN_LUM_THRESHOLD)
                        dark_count = insert_vib_entry(dark, dark_count, TOPN_HALF, vibrance, cr, cg, cb);
                else
                        bright_count = insert_vib_entry(bright, bright_count, TOPN_HALF, vibrance, cr, cg, cb);
        }

        palette->count = 0;
        for (int i = dark_count - 1; i >= 0 && palette->count < TOPN_VIBRANT_N; i--) {
                palette->colors[palette->count++] = (PixelData){
                    .r = dark[i].r, .g = dark[i].g, .b = dark[i].b, .a = 255};
        }
        for (int i = 0; i < bright_count && palette->count < TOPN_VIBRANT_N; i++) {
                palette->colors[palette->count++] = (PixelData){
                    .r = bright[i].r, .g = bright[i].g, .b = bright[i].b, .a = 255};
        }
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
                                double dr = (double)samples[i].r - centroids[k].r;
                                double dg = (double)samples[i].g - centroids[k].g;
                                double db = (double)samples[i].b - centroids[k].b;
                                double dist = dr * dr + dg * dg + db * db;
                                if (dist < min_dist) {
                                        min_dist = dist;
                                        best_k = k;
                                }
                        }
                        assignments[i] = best_k;
                }

                memset(sum_r, 0, sizeof(sum_r));
                memset(sum_g, 0, sizeof(sum_g));
                memset(sum_b, 0, sizeof(sum_b));
                memset(count, 0, sizeof(count));

                for (int i = 0; i < num_samples; i++) {
                        int k = assignments[i];
                        sum_r[k] += samples[i].r;
                        sum_g[k] += samples[i].g;
                        sum_b[k] += samples[i].b;
                        count[k]++;
                }

                for (int k = 0; k < 3; k++) {
                        if (count[k] > 0) {
                                centroids[k].r = (unsigned char)(sum_r[k] / count[k]);
                                centroids[k].g = (unsigned char)(sum_g[k] / count[k]);
                                centroids[k].b = (unsigned char)(sum_b[k] / count[k]);
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

static void check_if_bright_pixel(unsigned char r, unsigned char g, unsigned char b, bool *found)
{
        // Match main branch logic
        unsigned char lum = (unsigned char)(0.2126f * r + 0.7152f * g + 0.0722f * b);
        if (lum > 60 && !(r < g + 20 && r > g - 20 && g < b + 20 && g > b - 20) && !(r > 150 && g > 150 && b > 150)) {
                *found = true;
        }
}

static void generate_kmeans_clustering_palette(ColorPalette *palette, const unsigned char *pixels, int w, int h, int channels)
{
        if (w <= 0 || h <= 0 || pixels == NULL) {
                palette->count = 0;
                return;
        }

        int step_y = h / 16;
        int step_x = w / 16;
        if (step_y < 1)
                step_y = 1;
        if (step_x < 1)
                step_x = 1;

        PixelData samples[1024];
        int num_samples = 0;

        // Pass 1: Bright pixels
        for (int y = 0; y < h && num_samples < 1024; y += step_y) {
                for (int x = 0; x < w && num_samples < 1024; x += step_x) {
                        int idx = (y * w + x) * channels;
                        unsigned char r = pixels[idx];
                        unsigned char g = pixels[idx + 1];
                        unsigned char b = pixels[idx + 2];
                        bool bright = false;
                        check_if_bright_pixel(r, g, b, &bright);
                        if (bright) {
                                samples[num_samples++] = (PixelData){r, g, b, 255};
                        }
                }
        }

        // Pass 2: All pixels (if no bright pixels found)
        if (num_samples == 0) {
                for (int y = 0; y < h && num_samples < 1024; y += step_y) {
                        for (int x = 0; x < w && num_samples < 1024; x += step_x) {
                                int idx = (y * w + x) * channels;
                                samples[num_samples++] = (PixelData){pixels[idx], pixels[idx + 1], pixels[idx + 2], 255};
                        }
                }
        }

        PixelData centroids[3];
        run_kmeans(samples, num_samples, centroids);

        palette->count = 3;
        for (int i = 0; i < 3; i++) {
                palette->colors[i] = centroids[i];
                palette->colors[i].a = 255;
        }
}

static float pixel_luminance(const PixelData *c)
{
        return 0.2126f * c->r + 0.7152f * c->g + 0.0722f * c->b;
}

static void sort_palette_by_luminosity(ColorPalette *palette)
{
        for (int a = 1; a < palette->count; a++) {
                PixelData key = palette->colors[a];
                float key_lum = pixel_luminance(&key);
                int b = a - 1;
                while (b >= 0 && pixel_luminance(&palette->colors[b]) > key_lum) {
                        palette->colors[b + 1] = palette->colors[b];
                        b--;
                }
                palette->colors[b + 1] = key;
        }
}

void generate_all_visualizer_palettes(Model *model, int height)
{
        const UISettings *ui = &model->state.settings;

        PixelData base_color = {0, 0, 0, 255};
        if (ui->colorMode == COLOR_MODE_ALBUM) {
                base_color = ui->color;
        } else if (ui->theme.trackview_visualizer.type == COLOR_TYPE_RGB) {
                base_color = ui->theme.trackview_visualizer.rgb;
        }

        generate_legacy_palette(&model->state.ui.visualizer_palettes[VIZ_LIGHTEN], base_color, height, 0);
        generate_legacy_palette(&model->state.ui.visualizer_palettes[VIZ_REVERSED], base_color, height, 1);
        generate_legacy_palette(&model->state.ui.visualizer_palettes[VIZ_FLAT], base_color, height, 1);

        const unsigned char *cover = NULL;
        int cover_w = 0, cover_h = 0, cover_ch = 0;
        if (model->songdata && model->songdata->cover) {
                cover = model->songdata->cover;
                cover_w = model->songdata->coverWidth;
                cover_h = model->songdata->coverHeight;
                cover_ch = 4;
        }

        if (cover && cover_w > 0 && cover_h > 0) {
                generate_kmeans_palette(&model->state.ui.visualizer_palettes[VIZ_LUM_VIBRANT], cover, cover_w, cover_h, cover_ch);
                sort_palette_by_luminosity(&model->state.ui.visualizer_palettes[VIZ_LUM_VIBRANT]);
                generate_binning_palette(&model->state.ui.visualizer_palettes[VIZ_BINNING], cover, cover_w, cover_h, cover_ch);
                if (model->state.ui.visualizer_palettes[VIZ_BINNING].count > 8)
                        model->state.ui.visualizer_palettes[VIZ_BINNING].count = 8;
                sort_palette_by_luminosity(&model->state.ui.visualizer_palettes[VIZ_BINNING]);
                generate_topn_vibrant_palette(&model->state.ui.visualizer_palettes[VIZ_VIBRANT], cover, cover_w, cover_h, cover_ch);
                generate_kmeans_clustering_palette(&model->state.ui.visualizer_palettes[VIZ_KMEANS_CLUSTERING], cover, cover_w, cover_h, cover_ch);
        } else {
                model->state.ui.visualizer_palettes[VIZ_LUM_VIBRANT].count = 0;
                model->state.ui.visualizer_palettes[VIZ_BINNING].count = 0;
                model->state.ui.visualizer_palettes[VIZ_VIBRANT].count = 0;
                model->state.ui.visualizer_palettes[VIZ_KMEANS_CLUSTERING].count = 0;
        }
}

static sound_system_t *sound_s;

static float *fft_input = NULL;
static fftwf_plan fft_plan = NULL;
static fftwf_complex *fft_output = NULL;
static ma_uint32 sample_rate = 0;
static float bar_height[MAX_BARS] = {0.0f};
static float display_magnitudes[MAX_BARS] = {0.0f};
static float magnitudes[MAX_BARS] = {0.0f};
static float dBFloor = -60.0f;
static float dBCeil = -18.0f;
static float emphasis = 1.3f;
static float fast_attack = 0.6f;
static float decay = 0.14f;
static float slow_attack = 0.15f;
static int visualizer_bar_mode = 2;
static int max_thin_bars_in_auto_mode = 20;
static int prev_fft_size = 0;

void clear_magnitudes(int num_bars, float *magnitudes)
{
        for (int i = 0; i < num_bars; i++) {
                magnitudes[i] = 0.0f;
        }
}

void generate_blackman_harris_window(float *window, int buffer_size)
{
    const float alpha0 = 0.35875f;
    const float alpha1 = 0.48829f;
    const float alpha2 = 0.14128f;
    const float alpha3 = 0.01168f;

    float denom = (float)(buffer_size - 1);

    for (int i = 0; i < buffer_size; i++) {
        float x = 2.0f * M_PI * i / denom;

        window[i] =
            alpha0
            - alpha1 * cosf(x)
            + alpha2 * cosf(2.0f * x)
            - alpha3 * cosf(3.0f * x);
    }
}

void apply_blackman_harris(float *restrict data,
                  const float *restrict window,
                  int n)
{
    for (int i = 0; i < n; i++)
        data[i] *= window[i];
}

// Fill center freqs for 1/3-octave bands, given min/max freq and num_bands
void compute_band_centers(float min_freq, float sample_rate, int num_bands,
                          float *center_freqs)
{
        if (!center_freqs || num_bands <= 0 || min_freq <= 0 || sample_rate <= 0)
                return;

        float nyquist = sample_rate * 0.5f;
        float octave_fraction = 1.0f / 3.0f; // 1/3 octave
        float factor = powf(2.0f, octave_fraction);
        float f = min_freq;

        // Ensure we don't exceed the Nyquist frequency
        for (int i = 0; i < num_bands; i++) {
                if (f > nyquist) {
                        center_freqs[i] =
                            nyquist; // Clamp remaining bands at Nyquist
                } else {
                        center_freqs[i] = f;
                        f *= factor;

                        // Safeguard against overflow in case 'f' grows too
                        // large
                        if (f > nyquist) {
                                f = nyquist;
                        }
                }
        }
}

void fill_eq_bands(const fftwf_complex *fft_output, int buffer_size,
                   float sample_rate, float *band_db, int num_bands,
                   const float *center_freqs)
{
        // Basic input checks
        if (!fft_output || !band_db || !center_freqs || buffer_size <= 0 ||
            num_bands <= 0 || sample_rate <= 0.0f)
                return;

        // Guard against potential overflow in bin count computation
        if (buffer_size > INT_MAX - 1)
                return;

        int num_bins = buffer_size / 2 + 1;

        // Safe bin_spacing computation
        float bin_spacing = sample_rate / (float)buffer_size;
        if (bin_spacing <= 0.0f || !isfinite(bin_spacing))
                return;

        // Prevent division by zero in normalization
        float norm_factor = (float)buffer_size;
        if (norm_factor <= 0.0f)
                return;

        // Frequency window width for 1/3-octave bands
        const float width = powf(2.0f, 1.0f / 6.0f); // +/-1/6 octave

        // Pink noise correction
        const float correction_per_octave = 3.0f;
        const float max_freq_for_correction = 10000.0f;
        const float nyquist = sample_rate * 0.5f;

        // Make sure reference_freq is safe
        float reference_freq = fmaxf(center_freqs[0], 1.0f);
        if (!isfinite(reference_freq) || reference_freq <= 0.0f)
                reference_freq = 1.0f;

        for (int i = 0; i < num_bands; i++) {
                float center = center_freqs[i];

                if (!isfinite(center) || center <= 0.0f || center > nyquist) {
                        band_db[i] = -INFINITY;
                        continue;
                }

                float lo = center / width;
                float hi = center * width;

                // Avoid integer overflows in bin index computation
                int bin_lo = (int)ceilf(lo / bin_spacing);
                int bin_hi = (int)floorf(hi / bin_spacing);

                bin_lo = (bin_lo < 0) ? 0 : bin_lo;
                bin_hi = (bin_hi >= num_bins) ? num_bins - 1 : bin_hi;
                bin_hi = (bin_hi < bin_lo) ? bin_lo : bin_hi;

                float sum_sq = 0.0f;
                int count = 0;

                for (int k = bin_lo; k <= bin_hi; k++) {
                        if (k < 0 || k >= num_bins)
                                continue;

                        float real = crealf(fft_output[k]) / norm_factor;
                        float imag = cimagf(fft_output[k]) / norm_factor;
                        float mag = sqrtf(real * real + imag * imag);
                        sum_sq += mag * mag;
                        count++;
                }

                float rms = (count > 0) ? sqrtf(sum_sq / count)
                                        : 1e-9f; // Small nonzero floor
                band_db[i] = 20.0f * log10f(rms);

                // Pink noise EQ compensation
                float freq = fminf(center, max_freq_for_correction);
                float octaves_above_ref = log2f(freq / reference_freq);
                float correction =
                    fmaxf(octaves_above_ref, 0.0f) * correction_per_octave;
                band_db[i] += correction;
        }
}

// Sign-extend s24
ma_int32 unpack_s24_format(const ma_uint8 *p)
{
        ma_int32 sample = p[0] | (p[1] << 8) | (p[2] << 16);
        if (sample & 0x800000)
                sample |= ~0xFFFFFF;
        return sample;
}

int normalize_audio_samples(const void *audio_buffer, float *fft_input,
                            int buffer_size, int bit_depth)
{
        if (bit_depth == 8) {
                const uint8_t *buf = (const uint8_t *)audio_buffer;
                for (int i = 0; i < buffer_size; ++i)
                        fft_input[i] = ((float)buf[i] - 127.0f) / 128.0f;
        } else if (bit_depth == 16) {
                const int16_t *buf = (const int16_t *)audio_buffer;
                for (int i = 0; i < buffer_size; ++i)
                        fft_input[i] = (float)buf[i] / 32768.0f;
        } else if (bit_depth == 24) {
                const uint8_t *buf = (const uint8_t *)audio_buffer;
                for (int i = 0; i < buffer_size; ++i) {
                        int32_t sample = unpack_s24_format(buf + i * 3);
                        fft_input[i] = (float)sample / 8388608.0f;
                }
        } else if (bit_depth == 32) {
                const float *buf = (const float *)audio_buffer;
                for (int i = 0; i < buffer_size; ++i)
                        fft_input[i] = buf[i];
        } else {
                // Unsupported bit depth
                return -1;
        }

        return 0;
}

void calc_magnitudes(int height, int num_bars, void *audio_buffer, int bit_depth,
                     float *fft_input, fftwf_complex *fft_output, int fft_size,
                     float *magnitudes, fftwf_plan plan,
                     float *display_magnitudes)
{
        // Only execute when we get the signal that we have enough samples
        // (fft_size)
        if (!sound_system_is_buffer_ready(sound_s))
                return;

        if (!audio_buffer) {
                fprintf(stderr, "Audio buffer is NULL.\n");
                return;
        }

        sound_system_set_buffer_ready(sound_s, false);

        normalize_audio_samples(audio_buffer, fft_input, fft_size, bit_depth);

        // Apply Blackman Harris window function
        apply_blackman_harris(fft_input, blackman_harris_window, fft_size);

        // Compute fast fourier transform
        fftwf_execute(plan);

        // Clear previous magnitudes
        clear_magnitudes(MAX_BARS, magnitudes);

        float center_freqs[num_bars];

        float min_freq = 25.0f;
        float audible_half = 10000.0f;
        float max_freq = fmin(audible_half, 0.5f * sample_rate);
        float octave_fraction = 1.0f / 3.0f;
        int used_bars = floor(log2(max_freq / min_freq) / octave_fraction) +
                        1; // How many bars are actually in use, given we
                           // increase with 1/3 octave per bar

        // Compute center frequencies for EQ bands
        compute_band_centers(min_freq, max_freq, num_bars, center_freqs);

        // Fill magnitudes for EQ bands from FFT output
        fill_eq_bands(fft_output, fft_size, sample_rate, magnitudes, num_bars,
                      center_freqs);

        // Map magnitudes (in dB) to bar heights with gating and emphasis
        // (pow/gated)
        for (int i = 0; i < used_bars; ++i) {
                float db = magnitudes[i];
                if (db < dBFloor)
                        db = dBFloor;
                if (db > dBCeil)
                        db = dBCeil;
                float ratio = (db - dBFloor) / (dBCeil - dBFloor);
                ratio = powf(ratio, emphasis);
                if (ratio < 0.1f)
                        bar_height[i] = 0.0f; // Gate out tiny bars
                else
                        bar_height[i] = ratio * height;
        }

        float snap_threshold = 0.2f * height;

        // Smoothly update display magnitudes with attack/decay and snap
        // threshold
        for (int i = 0; i < used_bars; ++i) {
                float current = display_magnitudes[i];
                float target = bar_height[i];
                float delta = target - current;
                if (delta > snap_threshold)
                        display_magnitudes[i] +=
                            delta * fast_attack; // SNAP on big hits
                else if (delta > 0)
                        display_magnitudes[i] += delta * slow_attack;
                else
                        display_magnitudes[i] += delta * decay;
        }
}

char *upward_motion_chars_block[] = {" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};

char *upward_motion_chars_braille[] = {" ", "⣀", "⣀", "⣤", "⣤",
                                       "⣶", "⣶", "⣿", "⣿"};

char *inbetween_chars_rising[] = {" ", "⣠", "⣠", "⣴", "⣴", "⣾", "⣾", "⣿", "⣿"};

char *inbetween_chars_falling[] = {" ", "⡀", "⡀", "⣄", "⣄", "⣦", "⣦", "⣷", "⣷"};

char *get_upward_motion_char(int level, bool braille)
{
        if (level < 0 || level > 8) {
                level = 8;
        }
        if (braille)
                return upward_motion_chars_braille[level];
        else
                return upward_motion_chars_block[level];
}

char *get_inbetweend_motion_char(float magnitude_prev, float magnitude_next,
                                 int prev, int next)
{
        if (prev < 0)
                prev = 0;
        if (prev > 8)
                prev = 8;
        if (next < 0)
                next = 0;
        if (next > 8)
                next = 8;

        if (magnitude_next > magnitude_prev)
                return inbetween_chars_rising[prev];
        else if (magnitude_next < magnitude_prev)
                return inbetween_chars_falling[prev];
        else
                return upward_motion_chars_braille[prev];
}

char *get_inbetween_char(float prev, float next)
{
        int first_decimal_digit = (int)(fmod(prev * 10, 10));
        int second_decimal_digit = (int)(fmod(next * 10, 10));

        return get_inbetweend_motion_char(prev, next, first_decimal_digit,
                                          second_decimal_digit);
}

void free_visuals(void)
{
        if (fft_input != NULL) {
                free(fft_input);
                fft_input = NULL;
        }
        if (fft_output != NULL) {
                fftwf_free(fft_output);
                fft_output = NULL;
        }
        if (fft_plan != NULL) {
                fftwf_destroy_plan(fft_plan);
                fft_plan = NULL;
        }

        if (blackman_harris_window != NULL) {
                free(blackman_harris_window);
        }
}

/*
# Visualizer Palettes — Implementation Details

### Legacy palette (modes 0, 2)

Pre-bakes the existing math functions, preserving pixel-perfect parity:

- Mode 0: `increase_luminosity_for_height(base, height, j, false)` — bright top-to-bottom (lighten)
- Mode 1: `increase_luminosity_for_height(base, height, j, true)` — bright bottom-to-top (reversed)

Palette has `min(height, 16)` entries (typically 4–5 for a default visualizer height of 5).

### Luminance-bucketed vibrance palette (mode 1)

Replaced the original K-Means approach (which suffered from centroid collapse and poor luminance spread) with a single-pass luminance-bucketed vibrance picker.

**Algorithm:**

1. Sample pixels uniformly through the image (~1600 samples max, step = total_pixels / 1600)
2. Compute integer luminance: `(54*R + 183*G + 19*B) >> 8` — BT.709 coefficients, proper 0–255 range
3. Map luminance to 8 equal buckets via `lum >> 5` (bucket 0 = lum 0–31, bucket 7 = lum 224–255)
4. For each pixel, compute vibrance score: `saturation * 3 + brightness` where `saturation = max(R,G,B) - min(R,G,B)` and `brightness = max(R,G,B)`
5. Track the highest-vibrance pixel per bucket
6. Output all non-empty buckets in order (0→7 = dark→light)

**Properties:**
- O(n) single-pass, integer-only math — no iterative convergence
- Guaranteed luminance spread by bucket construction (8 equal bands of 32 luminance levels)
- Within each band, selects the pixel with best combined saturation+brightness
- No sorting needed — output order is inherently dark→light by bucket index
- Typically produces 5–8 entries depending on image luminance range

### Color binning palette (mode 3)

- Quantizes all pixels into a 32×32×32 grid (`BIN_SHIFT = 5`)
- Extracts top 12 most populated bins, then caps to 8
- Sorted by luminosity after generation via `sort_palette_by_luminosity()`

### Two-band vibrant palette (mode 4)

The most complex generator. Addresses the problem that the vibrance formula `chroma * (1 - |lum - 128| / 128)` heavily favors mid-bright pixels, excluding dark but representative album colors.

**Algorithm:**

1. Two independent top-4 selectors: `dark[]` for pixels with `lum < 80`, `bright[]` for `lum >= 80`
2. Single pass through sampled pixels. Each pixel routes to the appropriate band based on luminance
3. Within each band, insertion sorted by vibrance score (highest first, so `band[0]` = most vibrant)
4. Minimum RGB distance check (threshold = 60) — rejects pixels too similar to already-selected entries in the same band
5. Palette assembly with a **peak-vibrance-at-midpoint** layout:
   - Indices 0–3: dark band, **reversed** (least vibrant → most vibrant as magnitude increases)
   - Indices 4–7: bright band, **as-is** (most vibrant → least vibrant as magnitude increases)
6. No shared sort helper needed — ordering is achieved through assembly direction

**Palette layout relative to bar magnitude:**

```
magnitude:  low ─────────────────────────────────── high
            │                                         │
dark:       least vibrant ──────────► most vibrant   │
            └────────────────────────┘                │
                                  ┌────────────────────┘
bright:                           most vibrant ──► least vibrant
```

Vibrance peaks at the midpoint (transition from dark to bright band) and tapers at both ends. Visually: quiet bars are muted/dark, mid-volume bars are the most vivid, and the loudest bars are bright but less saturated.

*/

void draw_spectrum_to_buf(const Model *model, DrawBuffer *buf, int row, int col,
                          int height, int num_bars, int visualizer_width,
                          float *magnitudes)
{
        // Resolve base color
        PixelData color = {0, 0, 0, 255};
        const UISettings *ui = &model->state.settings;

        if (ui->colorMode == COLOR_MODE_ALBUM) {
                color = model->songdata && model->songdata->cover ? model->songdata->kmeans_palette[0] : ui->color;
       } else if (ui->colorMode == COLOR_MODE_ALBUM_ONE) {
                color = ui->color;
        } else if (ui->colorMode == COLOR_MODE_THEME &&
                   ui->theme.trackview_visualizer.type == COLOR_TYPE_RGB) {
                color = ui->theme.trackview_visualizer.rgb;
        }

        // Resolve pre-computed palette for modes 1-6
        const ColorPalette *palette = NULL;
        if (ui->visualizer_mode < VIZ_OFF) {
                palette = &model->state.ui.visualizer_palettes[ui->visualizer_mode];
        } else {
                return;
        }

        bool brailleMode = ui->visualizerBrailleMode;
        bool is_playing = (sound_system_get_state(sound_sys) == SOUND_STATE_PLAYING);
        bool wide_bar = (visualizer_bar_mode == 1 ||
                         (visualizer_bar_mode == 2 &&
                          visualizer_width > max_thin_bars_in_auto_mode));
        int bar_chars = wide_bar ? 3 : 2;

        // Clear if not playing
        if (!is_playing || model->is_paused || model->is_stopped) {
                CellStyle blank = cell_style_plain();
                for (int j = 0; j < height; j++)
                        draw_buffer_set_string_truncated(buf, row + j, col,
                                                         "", visualizer_width * bar_chars, blank);
                return;
        }

        // Draw each color
        for (int j = height; j > 0; j--) {
                int draw_row = row + height - j;

                // Row color
                PixelData row_color = color;

                if (palette != NULL && palette->count > 0 &&
                    (color.r != 0 || color.g != 0 || color.b != 0)) {
                        int pidx;
                        if (ui->visualizer_mode == VIZ_LIGHTEN) {

                                row_color = increase_luminosity_for_height(color, height, j, false);

                        } else if (ui->visualizer_mode == VIZ_REVERSED) {

                                row_color = increase_luminosity_for_height(color, height, j, true);

                        } else if (ui->visualizer_mode == VIZ_FLAT) {

                                row_color = color;

                        } else {
                                // Album-art: spread palette colors across height
                                // Use the full palette by stepping through it evenly
                                if (height >= palette->count)
                                        pidx = (j - 1) * palette->count / height;
                                else
                                        pidx = (j - 1) % palette->count;
                                if (pidx >= palette->count)
                                        pidx = palette->count - 1;

                                row_color = palette->colors[pidx];
                        }
                }

                CellStyle row_style;
                row_style = cell_style_fg(row_color);

                CellStyle from_theme = cell_style_from_theme(ui->theme.trackview_visualizer);
                if (from_theme.isAnsi)
                        row_style = from_theme;

                // Draw each bar in this row
                int draw_col = col;

                for (int i = 0; i < num_bars; i++) {

                        CellStyle bar_style = row_style;
                        if (ui->visualizer_mode != VIZ_LIGHTEN && ui->visualizer_mode != VIZ_REVERSED &&  ui->visualizer_mode != VIZ_FLAT &&
                            palette != NULL && palette->count > 0 &&
                            (color.r != 0 || color.g != 0 || color.b != 0)) {
                                PixelData bar_color;
                                if (palette->count == 1 || height <= 1) {
                                        bar_color = palette->colors[0];
                                } else {
                                        // Calculate the target position in the palette based on the bar's current magnitude.
                                        // This maps the magnitude from [1, height] to the palette index range [0, count-1].
                                        float scaled;
                                        if (magnitudes[i] <= 1.0f)
                                                scaled = 0.0f;
                                        else if (magnitudes[i] >= (float)height)
                                                scaled = (float)(palette->count - 1);
                                        else
                                                scaled = (magnitudes[i] - 1.0f) *
                                                          (palette->count - 1) /
                                                          (height - 1);

                                        // Perform linear interpolation between the two nearest palette entries.
                                        int lo = (int)scaled;
                                        int hi = lo + 1;
                                        if (hi >= palette->count) hi = palette->count - 1;

                                        // 'frac' is the interpolation factor [0, 1] between color at 'lo' and color at 'hi'.
                                        float frac = scaled - (float)lo;
                                        PixelData c0 = palette->colors[lo];
                                        PixelData c1 = palette->colors[hi];
                                        bar_color = (PixelData){
                                                .r = (unsigned char)(c0.r + (int)((c1.r - c0.r) * frac)),
                                                .g = (unsigned char)(c0.g + (int)((c1.g - c0.g) * frac)),
                                                .b = (unsigned char)(c0.b + (int)((c1.b - c0.b) * frac)),
                                                .a = 255
                                        };
                                }
                                if (ui->colorMode != COLOR_MODE_DEFAULT || ui->theme.trackview_visualizer.ansiIndex >= 100)
                                        bar_style = cell_style_fg(bar_color);
                                else
                                        bar_style = cell_style_from_theme(ui->theme.trackview_visualizer);
                        }
                        // Braille left-side character
                        if (brailleMode) {
                                const char *left_ch = " ";
                                if (i == 0) {
                                        left_ch = " ";
                                } else if (magnitudes[i - 1] >= j) {
                                        left_ch = get_upward_motion_char(10, brailleMode);
                                } else if (magnitudes[i - 1] + 1 >= j) {
                                        left_ch = get_inbetween_char(magnitudes[i - 1], magnitudes[i]);
                                }
                                draw_buffer_set_string(buf, draw_row, draw_col, left_ch, bar_style);
                                draw_col++;
                        } else {
                                // non-braille spacer
                                draw_buffer_set_string(buf, draw_row, draw_col, " ", bar_style);
                                draw_col++;
                        }

                        // Bar character
                        const char *bar_ch;
                        if (magnitudes[i] >= j) {
                                bar_ch = get_upward_motion_char(10, brailleMode);
                        } else if (magnitudes[i] + 1 >= j) {
                                int first_decimal = (int)(fmod(magnitudes[i] * 10, 10));
                                bar_ch = get_upward_motion_char(first_decimal, brailleMode);
                        } else {
                                bar_ch = " ";
                        }

                        draw_buffer_set_string(buf, draw_row, draw_col, bar_ch, bar_style);
                        draw_col++;

                        // Wide bar second character
                        if (wide_bar) {
                                draw_buffer_set_string(buf, draw_row, draw_col, bar_ch, bar_style);
                                draw_col++;
                        }
                }
        }
}

void draw_spectrum_visualizer_to_buf(const Model *model, DrawBuffer *buf, sound_system_t *system, int row, int col, int height, int width)
{
        AppState *state = get_app_state();
        sound_s = system;

        int num_bars = width / 2;
        int visualizer_width = num_bars;
        visualizer_bar_mode = state->settings.visualizer_bar_mode;

        int bar_width = 2;

        if (visualizer_bar_mode == 1 ||
            (visualizer_bar_mode == 2 &&
             num_bars > max_thin_bars_in_auto_mode)) {
                num_bars *= 0.67f;
                bar_width = 3;
        }

        if (num_bars > MAX_BARS)
                num_bars = MAX_BARS;

        // Center the visualizer
        int extra_cols = width - (num_bars * bar_width);
        col += (extra_cols / 2) - 1;

        if (height <= 0 || num_bars <= 0) {
                return;
        }

        sound_system_update_audio_buffer(sound_s);

        int fft_size = sound_system_get_fft_size(sound_s);

        if (fft_size != prev_fft_size) {
                free_visuals();

                blackman_harris_window = malloc(sizeof(float) * fft_size);
                if (!blackman_harris_window)
                        return;

                generate_blackman_harris_window(blackman_harris_window, fft_size);

                memset(display_magnitudes, 0, sizeof(display_magnitudes));

                fft_input = (float *)malloc(sizeof(float) * fft_size);
                if (fft_input == NULL) {
                        for (int i = 0; i <= height; i++) {
                                printf("\n");
                        }
                        return;
                }

                fft_output = (fftwf_complex *)fftwf_malloc(
                    sizeof(fftwf_complex) * fft_size);
                if (fft_output == NULL) {
                        fftwf_free(fft_input);
                        fft_input = NULL;
                        for (int i = 0; i <= height; i++) {
                                printf("\n");
                        }
                        return;
                }

                if (fft_plan != NULL) {
                        fftwf_destroy_plan(fft_plan);
                        fft_plan = NULL;
                }

                fft_plan = fftwf_plan_dft_r2c_1d(fft_size, fft_input, fft_output, FFTW_ESTIMATE);
                prev_fft_size = fft_size;
        }

        int bit_depth = sound_system_get_bit_depth(sound_s);
        sample_rate = sound_system_get_sample_rate(sound_s);

        calc_magnitudes(height, num_bars, sound_system_get_audio_buffer(sound_s), bit_depth, fft_input,
                        fft_output, fft_size, magnitudes, fft_plan, display_magnitudes);

        draw_spectrum_to_buf(model, buf, row, col, height, num_bars,
                             visualizer_width, display_magnitudes);
}
