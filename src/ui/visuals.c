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

#include "sound/playback.h"
#include "sound/audiobuffer.h"

#include "utils/term.h"

#include <float.h>
#include <fftw3.h>
#include <math.h>
#include <complex.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_BARS 26  // Counting 1/3 octave per bar, 50hz-10000hz range

static float *fft_input = NULL;
static fftwf_complex *fft_output = NULL;
static ma_format format = ma_format_unknown;
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

void apply_blackman_harris(float *fft_input, int buffer_size)
{
        if (!fft_input ||
            buffer_size < 2) // Must be at least 2 to avoid division by zero
                return;

        const float alpha0 = 0.35875f;
        const float alpha1 = 0.48829f;
        const float alpha2 = 0.14128f;
        const float alpha3 = 0.01168f;

        float denom = (float)(buffer_size - 1);

        for (int i = 0; i < buffer_size; i++) {
                float fraction = (float)i / denom;
                float window = alpha0 - alpha1 * cosf(2.0f * M_PI * fraction) +
                               alpha2 * cosf(4.0f * M_PI * fraction) -
                               alpha3 * cosf(6.0f * M_PI * fraction);

                fft_input[i] *= window;
        }
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

                        float real = fft_output[k][0] / norm_factor;
                        float imag = fft_output[k][1] / norm_factor;
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
                        int32_t sample = unpack_s24(buf + i * 3);
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
        if (!is_buffer_ready())
                return;

        if (!audio_buffer) {
                fprintf(stderr, "Audio buffer is NULL.\n");
                return;
        }

        set_buffer_ready(false);

        normalize_audio_samples(audio_buffer, fft_input, fft_size, bit_depth);

        // Apply Blackman Harris window function
        apply_blackman_harris(fft_input, fft_size);

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

int get_bit_depth(ma_format format)
{

        if (format == ma_format_unknown)
                return -1;

        int bit_depth = 32;

        switch (format) {
        case ma_format_u8:
                bit_depth = 8;
                break;

        case ma_format_s16:
                bit_depth = 16;
                break;

        case ma_format_s24:
                bit_depth = 24;
                break;

        case ma_format_f32:
        case ma_format_s32:
                bit_depth = 32;
                break;
        default:
                break;
        }

        return bit_depth;
}

void print_spectrum(int row, int col, UISettings *ui, int height, int num_bars,
                    int visualizer_width, float *magnitudes)
{
        PixelData color = {0, 0, 0};

        if (ui->colorMode == COLOR_MODE_ALBUM) {
                color.r = ui->color.r;
                color.g = ui->color.g;
                color.b = ui->color.b;
        } else if (ui->colorMode == COLOR_MODE_THEME && ui->theme.trackview_visualizer.type == COLOR_TYPE_RGB) {
                color.r = ui->theme.trackview_visualizer.rgb.r;
                color.g = ui->theme.trackview_visualizer.rgb.g;
                color.b = ui->theme.trackview_visualizer.rgb.b;
        }

        int visualizer_color_type = ui->visualizer_color_type;
        bool brailleMode = ui->visualizerBrailleMode;

        PixelData tmp;

        bool is_playing = !(pb_is_paused() || pb_is_stopped());

        for (int j = height; j > 0 && !is_playing; j--) {
                printf("\033[%d;%dH", row, col);
                clear_rest_of_line();
        }

        for (int j = height; j > 0 && is_playing; j--) {
                printf("\033[%d;%dH", row + height - j, col);

                if (color.r != 0 || color.g != 0 || color.b != 0) {
                        if ((visualizer_color_type == 0 ||
                             visualizer_color_type == 2 ||
                             visualizer_color_type == 3)) {
                                if (visualizer_color_type == 0) {
                                        tmp = increase_luminosity(
                                            color, round(j * height * 4));
                                } else if (visualizer_color_type == 2) {
                                        tmp = increase_luminosity(
                                            color,
                                            round((height - j) * height * 4));
                                } else if (visualizer_color_type == 3) {
                                        tmp = get_gradient_color(color, j, height,
                                                                 1, 0.6f);
                                }
                        }
                }

                if (ui->colorMode == COLOR_MODE_ALBUM)
                        printf("\033[38;2;%d;%d;%dm", tmp.r, tmp.g, tmp.b);
                else if (ui->theme.trackview_visualizer.type == COLOR_TYPE_RGB) {
                        printf("\033[38;2;%d;%d;%dm", tmp.r, tmp.g, tmp.b);
                } else
                        apply_color(ui->colorMode, ui->theme.trackview_visualizer, tmp);

                for (int i = 0; i < num_bars; i++) {
                        if (ui->colorMode != COLOR_MODE_DEFAULT && visualizer_color_type == 1) {
                                tmp = (PixelData){
                                    color.r / 2, color.g / 2,
                                    color.b /
                                        2}; // Make colors half as bright before
                                            // increasing brightness
                                tmp = increase_luminosity(
                                    tmp, round(magnitudes[i] * 10 * 4));

                                printf("\033[38;2;%d;%d;%dm", tmp.r, tmp.g,
                                       tmp.b);
                        }

                        if (i == 0 && brailleMode) {
                                printf(" ");
                        } else if (i > 0 && brailleMode) {
                                if (magnitudes[i - 1] >= j) {
                                        printf("%s", get_upward_motion_char(
                                                         10, brailleMode));
                                } else if (magnitudes[i - 1] + 1 >= j) {
                                        printf("%s", get_inbetween_char(
                                                         magnitudes[i - 1],
                                                         magnitudes[i]));
                                } else {
                                        printf(" ");
                                }
                        }

                        if (!brailleMode) {
                                printf(" ");
                        }

                        if (magnitudes[i] >= j) {
                                printf("%s",
                                       get_upward_motion_char(10, brailleMode));
                                if (visualizer_bar_mode == 1 ||
                                    (visualizer_bar_mode == 2 &&
                                     visualizer_width > max_thin_bars_in_auto_mode))
                                        printf("%s", get_upward_motion_char(
                                                         10, brailleMode));
                        } else if (magnitudes[i] + 1 >= j) {
                                int first_decimal_digit =
                                    (int)(fmod(magnitudes[i] * 10, 10));
                                printf("%s",
                                       get_upward_motion_char(first_decimal_digit,
                                                              brailleMode));
                                if (visualizer_bar_mode == 1 ||
                                    (visualizer_bar_mode == 2 &&
                                     visualizer_width > max_thin_bars_in_auto_mode))
                                        printf("%s", get_upward_motion_char(
                                                         first_decimal_digit,
                                                         brailleMode));
                        } else {
                                printf(" ");
                                if (visualizer_bar_mode == 1 ||
                                    (visualizer_bar_mode == 2 &&
                                     visualizer_width > max_thin_bars_in_auto_mode))
                                        printf(" ");
                        }
                }
        }
        fflush(stdout);
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
}

void draw_spectrum_visualizer(int row, int col, int height, int width)
{
        AppState *state = get_app_state();

        int num_bars = state->uiState.num_progress_bars;
        int visualizer_width = state->uiState.num_progress_bars;
        visualizer_bar_mode = state->uiSettings.visualizer_bar_mode;

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
        col += extra_cols / 2;

        height -= 1;

        if (height <= 0 || num_bars <= 0) {
                return;
        }

        int fft_size = get_fft_size();

        if (fft_size != prev_fft_size) {
                free_visuals();

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
                prev_fft_size = fft_size;
        }

        fftwf_plan plan =
            fftwf_plan_dft_r2c_1d(fft_size, fft_input, fft_output, FFTW_ESTIMATE);

        get_current_format_and_sample_rate(&format, &sample_rate);

        int bit_depth = get_bit_depth(format);

        calc_magnitudes(height, num_bars, get_audio_buffer(), bit_depth, fft_input,
                        fft_output, fft_size, magnitudes, plan, display_magnitudes);

        print_spectrum(row, col, &(state->uiSettings), height, num_bars,
                       visualizer_width, display_magnitudes);

        fftwf_destroy_plan(plan);
}
