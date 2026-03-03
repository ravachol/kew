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
#include "common/common.h"

#include "data/theme.h"

#include "sound/sound_facade.h"

int create_sound_system(void)
{
        AppState *state = get_app_state();

        bool success = sound_system_create(&sound_sys) == SOUND_OK;

        if (success)
                sound_system_set_replay_gain_check_track_first(sound_sys, state->uiSettings.replayGainCheckFirst);

        return success;
}

void shutdown_sound_system(void)
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
        sound_system_switch_decoder(sound_sys);
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

        if (!ps->skipOutOfOrder)
                trigger_refresh();
}

void ensure_default_theme_pack()
{
        ensure_default_themes();
}
