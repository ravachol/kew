/**
 * @file audiobuffer.c
 * @brief Related to the buffer used by the visualizer.
 *
 * Provides an api for stopping, starting and so on.
 * and switching decoders.
 */
#include "audiobuffer.h"

#include <stdatomic.h>
#include <string.h>

static int fft_size = 2048;
static int hop_size = 512;
static int fft_size_milliseconds = 45;
static int write_head = 0;
static bool buffer_ready = false;

static float audio_buffer[MAX_BUFFER_SIZE];

#define VIZ_RB_SIZE 262144
#define VIZ_RB_MASK (VIZ_RB_SIZE - 1) // only valid if VIZ_RB_SIZE is power of two
static float viz_rb[VIZ_RB_SIZE];
static _Atomic int viz_rb_write = 0;
static _Atomic int viz_rb_read = 0;

int closest_power_of_two(int x)
{
        int n = 1;
        while (n < x)
                n <<= 1;
        return n;
}

bool is_buffer_ready(void)
{
        return buffer_ready;
}

void set_buffer_ready(bool val)
{
        buffer_ready = val;
}

// Sign-extend s24
ma_int32 unpack_s24(const ma_uint8 *p)
{
        ma_int32 sample = p[0] | (p[1] << 8) | (p[2] << 16);
        if (sample & 0x800000)
                sample |= ~0xFFFFFF;
        return sample;
}

void visualizer_ringbuffer_push(const float *src, ma_uint32 frames, ma_uint32 channels)
{
        ma_uint32 write = atomic_load_explicit(&viz_rb_write, memory_order_relaxed);
        ma_uint32 total = frames * channels;

        if (total > VIZ_RB_SIZE)
                total = VIZ_RB_SIZE;

        ma_uint32 pos = write & VIZ_RB_MASK;

        ma_uint32 first = VIZ_RB_SIZE - pos;

        if (first > total)
                first = total;

        memcpy(&viz_rb[pos], src, first * sizeof(float));

        if (total > first)
                memcpy(&viz_rb[0], src + first, (total - first) * sizeof(float));

        atomic_store_explicit(&viz_rb_write, (write + total) & VIZ_RB_MASK, memory_order_release);
}

void prepare_visualizer_audiobuffer(ma_uint32 sample_rate, ma_uint32 channels)
{
        ma_uint32 write = atomic_load_explicit(&viz_rb_write, memory_order_acquire);
        ma_uint32 read  = atomic_load_explicit(&viz_rb_read, memory_order_relaxed);
        ma_uint32 available = (write - read + VIZ_RB_SIZE) % VIZ_RB_SIZE;

        int want_fft_samples = (int)(fft_size_milliseconds * sample_rate / 1000.0f);
        fft_size = closest_power_of_two(want_fft_samples);
        if (fft_size > MAX_BUFFER_SIZE)
                fft_size = MAX_BUFFER_SIZE;

        ma_uint32 samples_per_frame = (ma_uint32)(sample_rate * 0.034f) * channels;
        hop_size = (ma_uint32)(sample_rate * 0.034f);

        if (available < samples_per_frame)
                return;

        // If lagging, skip ahead to stay in sync
        ma_uint32 period_samples = (ma_uint32)(sample_rate * 0.300f) * channels;
        if (available > period_samples) {
                read = (read + (available - period_samples)) & VIZ_RB_MASK;
                available = period_samples;
        }

        ma_uint32 to_consume = samples_per_frame;

        while (to_consume >= channels && write_head < fft_size) {
                float sum = 0.0f;
                for (ma_uint32 ch = 0; ch < channels; ch++) {
                        sum += viz_rb[(read + ch) & VIZ_RB_MASK];
                }
                read += channels;
                to_consume -= channels;
                audio_buffer[write_head++] = sum / channels;

                if (write_head >= fft_size) {
                        set_buffer_ready(true);
                        memmove(audio_buffer, audio_buffer + hop_size,
                                sizeof(float) * (fft_size - hop_size));
                        write_head -= hop_size;
                        break;
                }
        }

        atomic_store_explicit(&viz_rb_read, read, memory_order_relaxed);
}

void reset_audio_buffer(void)
{
        memset(audio_buffer, 0, sizeof(ma_int32) * MAX_BUFFER_SIZE);
        write_head = 0;
        set_buffer_ready(false);
}

void *get_audio_buffer(void)
{
        return audio_buffer;
}

int get_fft_size(void)
{
        return fft_size;
};
