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

void playback_safe_cleanup(void) {
        AppState *state = get_app_state();
        pthread_mutex_lock(&(state->data_source_mutex));
        cleanup_playback_device();
        pthread_mutex_unlock(&(state->data_source_mutex));
}

void playback_cleanup(void) {
        cleanup_playback_device();
}

void playback_switch_decoder(void) {
        switch_audio_implementation();
}

int playback_create(void) {
        return create_audio_device();
}

void playback_shutdown(void) {
        sound_shutdown();
}

void playback_unload_songs(UserData *user_data) {
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

void skip(void) {
        PlaybackState *ps = get_playback_state();

        set_current_implementation_type(NONE);

        set_repeat_enabled(false);

        audio_data.end_of_list_reached = false;

        if (!is_playing()) {
                switch_audio_implementation();
                ps->skipFromStopped = true;
        } else {
                set_skip_to_next(true);
        }

        if (!ps->skipOutOfOrder)
                trigger_refresh();
}

void playback_free_decoders(void) {
        reset_all_decoders();
}

void ensure_default_theme_pack() {
        ensure_default_themes();
}
