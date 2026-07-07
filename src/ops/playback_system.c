/**
 * @file playback_system.c
 * @brief Low-level audio system integration.
 *
 * Manages initialization, configuration, and teardown of
 * audio backends and decoders. Provides the connection between
 * playback operations and sound output subsystems.
 */

#include "playback_system.h"
#include "playback_state.h"

#include "common/appstate.h"

#include "data/theme.h"

#include "sound/sound_facade.h"

atomic_bool start_audio;

int should_start_playing(void)
{
        return atomic_load(&start_audio);
}

void start_playing(bool value)
{
        atomic_store(&start_audio, value);
}

int create_sound_system(void)
{
        Model *model = get_model();

        sound_result_t result = sound_system_create(&sound_sys);

        if (result >= 0) {
                sound_system_set_replay_gain_check_first(sound_sys, model->state.settings.replayGainCheckFirst);
                sound_system_set_always_crossfade(sound_sys, model->state.settings.always_crossfade, model->state.settings.fade_medium_ms);
                start_playing(true);
                atomic_store(&sound_sys->track_frames_sent, 0);
                sound_sys->total_frames = 0;
        }

        if (result == SOUND_NOTIFY_SWITCH)
                model->playbackState.notifySwitch = 1;

        return result;
}

void sound_system_shutdown(void)
{
        sound_system_destroy(&sound_sys);
}

void uninit_device(void)
{
        sound_system_uninit_device(sound_sys);

        PlaybackState *ps = get_playback_state();
        ps->loadedNextSong = false;
        ps->waitingForNext = true;
}

void switch_decoder(void)
{
        Model *model = get_model();

        SongData *current_song = sound_system_get_current_song(sound_sys);

        sound_result_t result = SOUND_OK;

        if (current_song)
                result = sound_system_switch_decoder(sound_sys, current_song->file_path);
        else
                result = sound_system_switch_decoder(sound_sys, NULL);

        if (result == SOUND_NOTIFY_SWITCH)
                model->playbackState.notifySwitch = 1;
}

void skip(void)
{
        PlaybackState *ps = get_playback_state();

        sound_system_stop_decoding(sound_sys);

        if (is_repeat_enabled())
                set_repeat_state(0);

        sound_system_set_end_of_list_reached(sound_sys, false);

        if (sound_system_get_state(sound_sys) != SOUND_STATE_PLAYING) {
                switch_decoder();
                ps->skipFromStopped = true;
        } else {
                sound_system_skip_to_next(sound_sys);
        }
}

