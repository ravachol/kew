/**
 * @file playback_system.c
 * @brief Low-level audio system integration.
 *
 * Manages initialization, configuration, and teardown of
 * audio backends and decoders. Provides the connection between
 * playback operations and sound output subsystems.
 */

#include "playback_system.h"

#include "common/appstate.h"
#include "common/common.h"
#include "sound/audiotypes.h"
#include "sound/decoders.h"
#include "sound/playback.h"
#include "sound/sound.h"

#include "data/song_loader.h"

void playback_safe_cleanup(void)
{
        AppState *state = get_app_state();
        pthread_mutex_lock(&(state->data_source_mutex));
        pb_cleanup_playback_device();
        pthread_mutex_unlock(&(state->data_source_mutex));
}

void playback_cleanup(void)
{
        pb_cleanup_playback_device();
}

void switch_audio_implementation(void)
{
        pb_switch_audio_implementation();
}

int create_playback_device(void)
{
        return pb_create_audio_device();
}

void sound_shutdown(void)
{
        pb_sound_shutdown();
}

void unload_songs(UserData *user_data)
{
        PlaybackState *ps = get_playback_state();

        if (!user_data->songdataADeleted) {
                user_data->songdataADeleted = true;
                unload_song_data(&(ps->loadingdata.songdataA));
        }
        if (!user_data->songdataBDeleted) {
                user_data->songdataBDeleted = true;
                unload_song_data(&(ps->loadingdata.songdataB));
        }
}

void skip(void)
{
        PlaybackState *ps = get_playback_state();

        set_current_implementation_type(NONE);

        set_repeat_enabled(false);

        audio_data.end_of_list_reached = false;

        if (!is_playing()) {
                pb_switch_audio_implementation();
                ps->skipFromStopped = true;
        } else {
                set_skip_to_next(true);
        }

        if (!ps->skipOutOfOrder)
                trigger_refresh();
}

void free_decoders(void)
{
        reset_all_decoders();
}

void ensure_default_theme_pack()
{
        ensure_default_themes();
}
