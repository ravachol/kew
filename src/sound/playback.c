
/**
 * @file playback.c
 * @brief Playback related functions.
 *
 * Provides an api for stopping, starting and so on.
 * and switching decoders.
 */

#include "playback.h"

#include "common/common.h"

#include "loader/song_loader.h"

#include "decoders.h"
#include "sound.h"
#include "sound_facade.h"

#ifdef USE_FAAD
#include "m4a.h"
#endif

#include <stdatomic.h>

static ma_device device = {0};
static bool device_initialized = false;

static bool seek_requested = false;
static float seek_percent = 0.0;
static double seek_elapsed;

static _Atomic bool EOF_reached = false;
static _Atomic bool switch_reached = false;
static _Atomic bool skip_to_next = false;

static enum decoder_type_t current_decoder_type = NONE;

static double seek_elapsed;

ma_uint64 last_cursor = 0;

double get_seek_elapsed(void)
{
        return seek_elapsed;
}

void set_seek_elapsed(double value)
{
        seek_elapsed = value;
}

float get_seek_percentage(void)
{
        return seek_percent;
}

bool is_seek_requested(void)
{
        return seek_requested;
}

bool is_device_initialized(void)
{
        return device_initialized;
}

void set_device_initialized(bool value)
{
        device_initialized = value;
}

void set_seek_requested(bool value)
{
        seek_requested = value;
}

void seek_percentage(float percent)
{
        seek_percent = percent;
        set_seek_requested(true);
}

bool pb_is_EOF_reached(void)
{
        return atomic_load(&EOF_reached);
}

void set_EOF_reached(void)
{
        atomic_store(&EOF_reached, true);
}

void pb_set_EOF_handled(void)
{
        atomic_store(&EOF_reached, false);
}

bool pb_is_paused(void)
{
        return (sound_s->state == SOUND_STATE_PAUSED);
}

bool pb_is_stopped(void)
{
        return (sound_s->state == SOUND_STATE_STOPPED);
}

bool pb_is_repeat_enabled(void)
{
        return (repeat_state == SOUND_STATE_REPEAT);
}

bool pb_is_playing(void)
{
        return ma_device_is_started(&device);
}

void stop_playback(void)
{
        if (ma_device_is_started(&device)) {
                ma_device_stop(&device);
        }

        if (sound_s->state != SOUND_STATE_PAUSED)
                sound_s->state = SOUND_STATE_STOPPED;
}

int sound_resume_playback(void)
{
        int result = 0;

        if (!ma_device_is_started(&device)) {
                if (ma_device_start(&device) != MA_SUCCESS) {
                        result = sound_create_audio_device();
                        ma_device_start(&device);
                }
        }

        if (result < 0) {
                sound_s->state = SOUND_STATE_STOPPED;
                return result;
        }

        sound_s->state = SOUND_STATE_PLAYING;

        return 0;
}

void pause_playback(void)
{
        if (ma_device_is_started(&device)) {
                ma_device_stop(&device);
        }

        sound_s->state = SOUND_STATE_PAUSED;
}

void toggle_pause_playback(void)
{
        if (ma_device_is_started(&device)) {
                pause_playback();
        } else if (pb_is_paused() || pb_is_stopped()) {
                sound_resume_playback();
        }
}

int init_playback_device(ma_context *context, sound_system_t *sound,
                         ma_device *device, ma_device_data_proc data_callback, void *pUserData)
{
        ma_result result;

        ma_device_config deviceConfig =
            ma_device_config_init(ma_device_type_playback);

        deviceConfig.playback.format = sound->format;
        deviceConfig.playback.channels = sound->channels;
        deviceConfig.sampleRate = sound->sample_rate;
        deviceConfig.dataCallback = data_callback;
        deviceConfig.pUserData = pUserData;

        sound_s->sample_rate = sound->sample_rate;
        sound_s->format = sound->format;
        sound_s->channels = sound->channels;

        result = ma_device_init(context, &deviceConfig, device);
        if (result != MA_SUCCESS) {
                set_error_message("Failed to initialize miniaudio device.");
                return -1;
        } else {
                device_initialized = true;
        }

        result = ma_device_start(device);

        if (result != MA_SUCCESS) {
                set_error_message("Failed to start miniaudio device.");
                return -1;
        }

        sound_s->state = SOUND_STATE_PLAYING;

        return 0;
}

void stop_decode_thread(void)
{
        atomic_store(&sound_s->decode_thread_running, false);

        sound_wake_up();

        if (sound_s->decode_thread_active) {
                pthread_join(sound_s->decode_thread, NULL);
                sound_s->decode_thread_active = false;
        }
}

void cleanup_playback_device(void)
{
        if (!device_initialized)
                return;

        set_switch_files(false);

        stop_playback();
        stop_decode_thread();
        reset_decoders();
        sound_ringbuffer_cleanup();
        ma_device_uninit(&device);
        memset(&device, 0, sizeof(device));
        device_initialized = false;
}

ma_device *get_device(void)
{
        return &device;
}

void get_current_format_and_sample_rate(ma_format *format, ma_uint32 *sample_rate)
{
        *format = ma_format_unknown;

        if (get_current_decoder_decoder_type() == BUILTIN) {
                ma_decoder *decoder = (ma_decoder *)get_current_decoder();

                if (decoder != NULL)
                        *format = decoder->outputFormat;
        } else if (get_current_decoder_decoder_type() == OPUS) {
                ma_libopus *decoder = (ma_libopus *)get_current_decoder();

                if (decoder != NULL)
                        *format = decoder->format;
        } else if (get_current_decoder_decoder_type() == VORBIS) {
                ma_libvorbis *decoder = (ma_libvorbis *)get_current_decoder();

                if (decoder != NULL)
                        *format = decoder->format;
        } else if (get_current_decoder_decoder_type() == WEBM) {
                ma_webm *decoder = (ma_webm *)get_current_decoder();

                if (decoder != NULL)
                        *format = decoder->format;
        } else if (get_current_decoder_decoder_type() == M4A) {
#ifdef USE_FAAD
                m4a_decoder *decoder = (m4a_decoder *)get_current_decoder();

                if (decoder != NULL)
                        *format = decoder->format;
#endif
        }

        *sample_rate = sound_s->sample_rate;
}

bool is_decoder_type_switch_reached(void)
{
        return atomic_load(&switch_reached) ? true : false;
}

void set_decoder_type_switch_reached(void)
{
        atomic_store(&switch_reached, true);
}

void set_decoder_type_switch_not_reached(void)
{
        atomic_store(&switch_reached, false);
}

enum decoder_type_t get_current_decoder_type(void)
{
        return current_decoder_type;
}

void set_current_decoder_type(enum decoder_type_t value)
{
        current_decoder_type = value;
}

bool is_skip_to_next(void)
{
        return atomic_load(&skip_to_next) ? true : false;
}

void set_skip_to_next(bool value)
{
        atomic_store(&skip_to_next, value);
}

void clear_current_track(void)
{
        if (ma_device_is_started(&device)) {
                // Stop the device (which stops playback)
                ma_device_stop(&device);
        }

        reset_decoders();
}
