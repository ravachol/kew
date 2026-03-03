/**
 * @file sound_facade.c
 * @brief Public facade interface for the audio subsystem.
 *
 * This header defines the high-level API for interacting with the audio
 * subsystem. It provides a simplified facade over the underlying audio
 * implementation, abstracting decoder management, device handling,
 * playback control, and configuration.
 *
 * Clients should include this header and use the exposed functions to
 * create, control, and destroy the sound system without needing knowledge
 * of the internal audio architecture.
 *
 * IMPORTANT: This is the only sound header file that should be included outside of the sound module.
 *            The sound module shouldn't use any of the other modules except for loader and utils modules.
 */

#include "sound_facade.h"

#include "loader/song_loader.h"

#include "decoders.h"
#include "playback.h"
#include "sound.h"
#include "sound/audiobuffer.h"
#include "volume.h"

sound_result_t sound_system_create(sound_system_t **out_system)
{
        if (!out_system)
                return SOUND_ERROR_INVALID_ARGUMENT;

        *out_system = NULL;

        if (sound_s)
                sound_system_destroy(&sound_s);

        int result = song_loader_init();

        if (result < 0)
                return SOUND_ERROR_BACKEND_FAILURE;

        sound_s = malloc(sizeof(sound_system_t));
        if (!sound_s)
                return SOUND_ERROR_INTERNAL;

        memset(sound_s, 0, sizeof(*sound_s));

        set_current_volume(1.0f);

        sound_s->volume = get_current_volume();
        sound_s->state = SOUND_STATE_STOPPED;

        result = sound_create_audio_device();

        if (result < 0) {
                sound_system_destroy(&sound_s);

                return SOUND_ERROR_BACKEND_FAILURE;
        }

        *out_system = sound_s;

        return SOUND_OK;
}

void sound_system_destroy(sound_system_t **system)
{
        sound_shutdown();

        song_loader_destroy();

        free(*system);
        *system = NULL;
}

sound_result_t sound_system_uninit_device(sound_system_t *system)
{
        (void)system;
        cleanup_playback_device();

        return SOUND_OK;
}

sound_result_t sound_system_switch_decoder(sound_system_t *system)
{
        if (!system)
                return SOUND_ERROR_NOT_INITIALIZED;

        int result = sound_switch_audio_implementation();

        if (result < 0)
                return SOUND_ERROR_BACKEND_FAILURE;

        return SOUND_OK;
}

sound_result_t sound_system_load(
    sound_system_t *system,
    const char *file_path,
    int is_first_decoder,
    int is_next_song)
{
        (void)system;

        sound_load_song(file_path, is_first_decoder, is_next_song);

        return SOUND_OK;
}

sound_result_t sound_system_unload_songs(sound_system_t *system)
{
        (void)system;

        song_loader_unload_songs();

        return SOUND_OK;
}

sound_result_t sound_system_set_volume(
    sound_system_t *system,
    float volume)
{
        if (volume > 1.0f) {
                volume = 1.0f;
        } else if (volume < 0.0f) {
                volume = 0.0f;
        }

        set_current_volume(volume);

        if (system) {
                system->volume = get_current_volume();
        }

        return SOUND_OK;
}

float sound_system_get_volume(const sound_system_t *system)
{
        if (!system)
                return 0.0f;

        return system->volume;
}

sound_result_t sound_system_play(sound_system_t *system)
{
        if (!system)
                return SOUND_ERROR_NOT_INITIALIZED;

        int result = sound_resume_playback();

        if (result < 0)
                return SOUND_ERROR_BACKEND_FAILURE;

        return SOUND_OK;
}

sound_result_t sound_system_pause(sound_system_t *system)
{
        if (!system)
                return SOUND_ERROR_NOT_INITIALIZED;

        pause_playback();
        if (pb_is_paused())
                system->state = SOUND_STATE_PAUSED;

        return SOUND_OK;
}

sound_result_t sound_system_stop(sound_system_t *system)
{
        if (!system)
                return SOUND_ERROR_NOT_INITIALIZED;

        stop_playback();
        if (pb_is_stopped())
                system->state = SOUND_STATE_STOPPED;

        return SOUND_OK;
}

sound_result_t sound_system_stop_decoding(sound_system_t *system)
{
        if (!system)
                return SOUND_ERROR_NOT_INITIALIZED;

        set_current_implementation_type(NONE);

        return SOUND_OK;
}

sound_result_t sound_system_skip_to_next(sound_system_t *system)
{
        if (!system)
                return SOUND_ERROR_NOT_INITIALIZED;

        set_skip_to_next(true);

        return SOUND_OK;
}

sound_result_t sound_system_switch_song_immediate(sound_system_t *system)
{
        if (!system)
                return SOUND_ERROR_NOT_INITIALIZED;

        sound_switch_track_immediate();

        return SOUND_OK;
}

sound_result_t sound_system_clear_current_track(sound_system_t *system)
{
        if (!system)
                return SOUND_ERROR_NOT_INITIALIZED;

        clear_current_track();

        return SOUND_OK;
}

sound_result_t sound_system_toggle_pause(sound_system_t *system)
{
        if (!system)
                return SOUND_ERROR_NOT_INITIALIZED;

        toggle_pause_playback();
        if (pb_is_stopped())
                system->state = SOUND_STATE_STOPPED;
        if (pb_is_paused())
                system->state = SOUND_STATE_PAUSED;
        if (pb_is_playing())
                system->state = SOUND_STATE_PLAYING;

        return SOUND_OK;
}

sound_result_t sound_system_seek_percentage(sound_system_t *system, float percent)
{
        if (!system)
                return SOUND_ERROR_NOT_INITIALIZED;

        if (can_decoder_seek(get_current_decoder()))
                seek_percentage(percent);

        return SOUND_OK;
}

sound_result_t sound_system_set_seek_elapsed(double seek_elapsed)
{
        if (!can_decoder_seek(get_current_decoder()))
                seek_elapsed = 0.0;

        set_seek_elapsed(seek_elapsed);

        return SOUND_OK;
}

int sound_system_get_restart_audio(const sound_system_t *system)
{
        if (!system)
                return 0;

        return atomic_load(&system->restart_audio);
}

sound_result_t sound_system_set_restart_audio(sound_system_t *system, int value)
{
        if (!system)
                return SOUND_ERROR_NOT_INITIALIZED;

        atomic_store(&system->restart_audio, value);

        return SOUND_OK;
}

int sound_system_is_end_of_list_reached(const sound_system_t *system)
{
        if (!system)
                return 1;

        return atomic_load(&system->end_of_list_reached);
}

sound_result_t sound_system_set_end_of_list_reached(sound_system_t *system, int value)
{
        if (!system)
                return SOUND_ERROR_NOT_INITIALIZED;

        atomic_store(&system->end_of_list_reached, value);

        return SOUND_OK;
}
double sound_system_get_seek_elapsed(void)
{
        return get_seek_elapsed();
}

int sound_system_is_switching_track(const sound_system_t *system)
{
        if (!system)
                return 0;

        return pb_is_impl_switch_reached();
}

int sound_system_is_EOF_reached(void)
{
        return pb_is_EOF_reached();
}

sound_playback_state_t sound_system_get_state(
    const sound_system_t *system)
{
        if (!system)
                return SOUND_STATE_STOPPED;

        return system->state;
}

int sound_system_get_bit_depth(const sound_system_t *system)
{
        if (!system)
                return 0;

        return sound_get_bit_depth(system->format);
}

sound_result_t sound_system_set_buffer_ready(const sound_system_t *system, int value)
{
        if (!system)
                return SOUND_ERROR_NOT_INITIALIZED;;

        set_buffer_ready(value);

        return SOUND_OK;
}

void *sound_system_get_audio_buffer(const sound_system_t *system)
{
        if (!system)
                return NULL;

        return get_audio_buffer();
}

int sound_system_get_fft_size(const sound_system_t *system)
{
        (void)system;

        return get_fft_size();
}

int sound_system_is_buffer_ready(const sound_system_t *system)
{
        (void)system;

        return is_buffer_ready();
}

int sound_system_is_current_song_deleted(const sound_system_t *system)
{
        if (!system)
                return 1;

        LoaderData *loader_data = get_loader_data();

        return (sound_is_using_slot_A())
                   ? loader_data->songdataADeleted == true
                   : loader_data->songdataBDeleted == true;
}

uint64_t sound_system_get_duration_ms(
    const sound_system_t *system)
{
        if (!system)
                return 0;

        SongData *song_data = sound_get_current_song_data();

        return song_data->duration * 1000;
}

int sound_system_get_repeat_state(void)
{

        return repeat_state;
}

int sound_system_no_song_loaded(const sound_system_t *system)
{
        if (!system)
                return 1;

        LoaderData *loader_data = get_loader_data();

        return (loader_data->songdataADeleted && loader_data->songdataBDeleted);
}

sound_result_t sound_system_set_repeat_state(int value)
{
        repeat_state = (sound_playback_repeat_state_t)value;

        return SOUND_OK;
}

sound_result_t sound_system_set_EOF_handled(const sound_system_t *system)
{
        if (!system)
                return SOUND_ERROR_NOT_INITIALIZED;

        pb_set_EOF_handled();

        return SOUND_OK;
}

SongData *sound_system_get_current_song(const sound_system_t *system)
{
        if (!system)
                return NULL;

        return sound_get_current_song_data();
}

sound_result_t sound_system_set_replay_gain_check_track_first(sound_system_t *system, int value)
{
        if (!system)
                return SOUND_ERROR_NOT_INITIALIZED;

        system->replay_gain_check_track_first = value;

        return SOUND_OK;
}

int sound_system_get_replay_gain_check_track_first(const sound_system_t *system)
{
        return system->replay_gain_check_track_first;
}

int sound_system_get_sample_rate(const sound_system_t *system)
{
        return system->sample_rate;
}
