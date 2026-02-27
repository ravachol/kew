
/**
 * @file playback.h
 * @brief Playback related functions.
 *
 * Provides an api for stopping, starting and so on.
 * and switching decoders.
 */
#include "common/appstate.h"
#include "common/common.h"

#include "decoders.h"
#include "sound.h"

#ifdef USE_FAAD
#include "m4a.h"
#endif

#include <stdatomic.h>

static ma_device device = {0};
static bool device_initialized = false;

static bool paused = false;
static bool repeat_enabled = false;

static bool seek_requested = false;
static float seek_percent = 0.0;
static double seek_elapsed;

static bool skip_to_next = false;

static _Atomic bool stopped = true;
static _Atomic bool EOF_reached = false;
static _Atomic bool switch_reached = false;

static enum AudioImplementation current_implementation = NONE;

static double seek_elapsed;

ma_uint64 last_cursor = 0;

static pthread_mutex_t switch_mutex = PTHREAD_MUTEX_INITIALIZER;

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
        seek_requested = true;
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
        return paused;
}

void set_paused(bool val)
{
        paused = val;
}

bool pb_is_stopped(void)
{
        return atomic_load(&stopped);
}

void set_stopped(bool val)
{
        atomic_store(&stopped, val);
}

bool pb_is_repeat_enabled(void)
{
        return repeat_enabled;
}

void set_repeat_enabled(bool value)
{
        repeat_enabled = value;
}

bool is_playing(void)
{
        return ma_device_is_started(&device);
}

void stop_playback(void)
{
        AppState *state = get_app_state();

        if (ma_device_is_started(&device)) {
                ma_device_stop(&device);
        }

        set_paused(false);
        set_stopped(true);

        if (state->currentView != TRACK_VIEW) {
                trigger_refresh();
        }
}

void sound_resume_playback(void)
{
        AppState *state = get_app_state();

        if (audio_data.restart) {
                audio_data.end_of_list_reached = false;
        }

        if (!ma_device_is_started(&device)) {
                if (ma_device_start(&device) != MA_SUCCESS) {
                        pb_create_audio_device();
                        ma_device_start(&device);
                }
        }

        set_paused(false);

        set_stopped(false);

        if (state->currentView != TRACK_VIEW) {
                trigger_refresh();
        }
}

void pause_playback(void)
{
        AppState *state = get_app_state();

        if (ma_device_is_started(&device)) {
                ma_device_stop(&device);
        }

        set_paused(true);

        if (state->currentView != TRACK_VIEW) {
                trigger_refresh();
        }
}

void toggle_pause_playback(void)
{
        if (ma_device_is_started(&device)) {
                pause_playback();
        } else if (pb_is_paused() || pb_is_stopped()) {
                sound_resume_playback();
        }
}

int init_playback_device(ma_context *context, AudioData *audio_data,
                         ma_device *device, ma_device_data_proc data_callback, void *pUserData)
{
        ma_result result;

        ma_device_config deviceConfig =
            ma_device_config_init(ma_device_type_playback);

        deviceConfig.playback.format = audio_data->format;
        deviceConfig.playback.channels = audio_data->channels;
        deviceConfig.sampleRate = audio_data->sample_rate;
        deviceConfig.dataCallback = data_callback;
        deviceConfig.pUserData = pUserData;

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

        set_paused(false);
        set_stopped(false);

        return 0;
}

void stop_decode_thread(void)
{
        atomic_store(&audio_data.decode_thread_running, false);

        pthread_mutex_lock(&audio_data.rb_mutex);
        pthread_cond_broadcast(&audio_data.rb_cond);
        pthread_mutex_unlock(&audio_data.rb_mutex);

        if (audio_data.decode_thread_active) {
                pthread_join(audio_data.decode_thread, NULL);
                audio_data.decode_thread_active = false;
        }
}

void safe_ringbuffer_reset(ma_pcm_rb* pcm_rb) {
    if (pcm_rb->rb.ownsBuffer) {
        ma_pcm_rb_uninit(pcm_rb);
    }
    memset(pcm_rb, 0, sizeof(*pcm_rb)); // reset the entire struct safely
}

void cleanup_playback_device(void)
{
        if (!device_initialized)
                return;

        stop_playback();
        stop_decode_thread();
        reset_decoders();
        safe_ringbuffer_reset(&audio_data.pcm_rb);
        ma_device_uninit(&device);
        memset(&device, 0, sizeof(device));
        device_initialized = false;
}

void pb_sound_shutdown()
{
        if (is_context_initialized()) {
                cleanup_playback_device();
                cleanup_audio_context();
        }
}

ma_device *get_device(void)
{
        return &device;
}

enum AudioImplementation get_current_implementation_type(void)
{
        return current_implementation;
}

void get_current_format_and_sample_rate(ma_format *format, ma_uint32 *sample_rate)
{
        *format = ma_format_unknown;

        if (get_current_implementation_type() == BUILTIN) {
                ma_decoder *decoder = (ma_decoder*)get_current_decoder();

                if (decoder != NULL)
                        *format = decoder->outputFormat;
        } else if (get_current_implementation_type() == OPUS) {
                ma_libopus *decoder = (ma_libopus*)get_current_decoder();

                if (decoder != NULL)
                        *format = decoder->format;
        } else if (get_current_implementation_type() == VORBIS) {
                ma_libvorbis *decoder = (ma_libvorbis*)get_current_decoder();

                if (decoder != NULL)
                        *format = decoder->format;
        } else if (get_current_implementation_type() == WEBM) {
                ma_webm *decoder = (ma_webm*)get_current_decoder();

                if (decoder != NULL)
                        *format = decoder->format;
        } else if (get_current_implementation_type() == M4A) {
#ifdef USE_FAAD
                ma_m4a *decoder = (ma_m4a*)get_current_decoder();

                if (decoder != NULL)
                        *format = decoder->format;
#endif
        }

        *sample_rate = get_audio_data()->sample_rate;
}

void execute_switch(AudioData *p_audio_data)
{
        p_audio_data->switch_files = false;
        switch_decoder();

        if (p_audio_data == NULL)
                return;

        p_audio_data->pUserData->current_song_data =
            (p_audio_data->currentFileIndex == 0)
                ? p_audio_data->pUserData->songdataA
                : p_audio_data->pUserData->songdataB;
        p_audio_data->totalFrames = 0;
        p_audio_data->currentPCMFrame = 0;

        set_seek_elapsed(0.0);

        set_EOF_reached();
}

bool pb_is_impl_switch_reached(void)
{
        return atomic_load(&switch_reached) ? true : false;
}

void set_impl_switch_reached(void)
{
        atomic_store(&switch_reached, true);
}

void set_impl_switch_not_reached(void)
{
        atomic_store(&switch_reached, false);
}

void set_current_implementation_type(enum AudioImplementation value)
{
        current_implementation = value;
}

bool is_skip_to_next(void)
{
        return skip_to_next;
}

void set_skip_to_next(bool value)
{
        skip_to_next = value;
}

void activate_switch(AudioData *p_audio_data)
{
        set_skip_to_next(false);

        if (!pb_is_repeat_enabled()) {
                pthread_mutex_lock(&switch_mutex);
                p_audio_data->currentFileIndex =
                    1 - p_audio_data->currentFileIndex; // Toggle between 0 and 1
                pthread_mutex_unlock(&switch_mutex);
        }

        p_audio_data->switch_files = true;
}

void clear_current_track(void)
{
        if (ma_device_is_started(&device)) {
                // Stop the device (which stops playback)
                ma_device_stop(&device);
        }

        reset_decoders();
}
