/**
 * @file playback_ops.c
 * @brief Core playback control API.
 *
 * Contains functions to control playback: play, pause, stop, seek,
 * volume adjustments, and track skipping. This module is UI-agnostic
 * and interacts directly with the playback state and audio backends.
 */

#include "common/appstate.h"

#include "playback_ops.h"

#include "common/common.h"
#include "playback_clock.h"
#include "playback_state.h"
#include "playback_system.h"
#include "playlist_ops.h"
#include "track_manager.h"

#include "sound/sound_facade.h"

#include "sys/sys_integration.h"

#include "utils/utils.h"

void resume_playback(void)
{
        sound_system_play(sound_sys);
}

void try_load_next(void)
{
        PlaybackState *ps = get_playback_state();
        Node *current = get_current_song();
        Node *try_next_song = get_try_next_song();

        ps->songHasErrors = false;
        ps->clearingErrors = true;

        if (try_next_song == NULL && current != NULL)
                try_next_song = current->next;
        else if (try_next_song != NULL)
        {
                try_next_song = try_next_song->next;
                set_try_next_song(try_next_song);
        }
        if (try_next_song != NULL) {
                ps->songLoading = true;
                load_song(try_next_song, false, false);
        } else {
                ps->clearingErrors = false;
        }
}

void pause_song(void)
{
        if (sound_system_get_state(sound_sys) != SOUND_STATE_PAUSED) {
                emit_string_property_changed("PlaybackStatus", "Paused");
                update_pause_time();
        }

        AppState *state = get_app_state();

        if (state->currentView != TRACK_VIEW) {
                trigger_refresh();
        }

        sound_system_pause(sound_sys);
}

void skip_to_begginning_of_song(void)
{
        reset_clock();

        if (get_current_song() != NULL) {
                sound_system_seek_percentage(sound_sys, 0);
                emit_seeked_signal(0.0);
        }
}

void prepare_if_skipped_silent(void)
{
        PlaybackState *ps = get_playback_state();

        if (ps->hasSilentlySwitched) {
                ps->skipping = true;
                ps->hasSilentlySwitched = false;

                reset_clock();

                sound_system_stop_decoding(sound_sys);

                if (get_repeat_state() == 1)
                        set_repeat_state(0);

                sound_system_set_end_of_list_reached(sound_sys, false);

                ps->skipping = false;
        }
}

void play(void)
{
        PlaybackState *ps = get_playback_state();

        int state = sound_system_get_state(sound_sys);

        if (state == SOUND_STATE_PAUSED) {
                set_total_pause_seconds(get_total_pause_seconds() + get_pause_seconds());
                emit_string_property_changed("PlaybackStatus", "Playing");
        } else if (state == SOUND_STATE_STOPPED) {
                emit_string_property_changed("PlaybackStatus", "Playing");
        }

        if (state == SOUND_STATE_STOPPED && !ps->hasSilentlySwitched) {
                skip_to_begginning_of_song();
        }

        resume_playback();

        if (ps->hasSilentlySwitched) {
                set_total_pause_seconds(0);
                prepare_if_skipped_silent();
        }
}

bool is_valid_audio_node(Node *node)
{
        if (!node)
                return false;
        if (node->id < 0)
                return false;
        if (!node->song.file_path ||
            strnlen(node->song.file_path, PATH_MAX) == 0)
                return false;

        return true;
}

int play_song(Node *node)
{
        PlaybackState *ps = get_playback_state();

        if (!is_valid_audio_node(node)) {
                fprintf(stderr, "Song is invalid.\n");
                return -1;
        }

        set_current_song(node);

        ps->skipping = true;
        ps->skipOutOfOrder = false;
        ps->songLoading = true;
        ps->forceSkip = false;

        ps->loadedNextSong = false;

        // Cancel starting from top
        if (ps->waitingForPlaylist || should_start_playing()) {
                ps->waitingForPlaylist = false;
                start_playing(true);

                if (is_shuffle_enabled())
                        reshuffle_playlist();
        }

        load_song(node, true, false);

        int max_num_tries = 50;
        int numtries = 0;

        while (!ps->loadedNextSong && numtries < max_num_tries) {
                c_sleep(100);
                numtries++;
        }

        if (ps->songHasErrors) {
                ps->songHasErrors = false;
                ps->forceSkip = true;

                if (node->next != NULL) {
                        return -1;
                }
        }

        reset_clock();
        skip();

        return 0;
}

void volume_change(int change_percent)
{
        int sound_volume = get_volume();
        sound_volume += change_percent;

        set_volume(sound_volume);
}

void skip_to_song(int id, bool start_playing)
{
        PlaybackState *ps = get_playback_state();

        if (ps->songLoading || !ps->loadedNextSong || ps->skipping || ps->clearingErrors)
                if (!ps->forceSkip)
                        return;

        PlayList *playlist = get_playlist();
        Node *found = NULL;

        find_node_in_list(playlist, id, &found);

        if (start_playing) {
                double total_pause_seconds = get_total_pause_seconds();
                double pause_seconds = get_total_pause_seconds();

                play();

                set_total_pause_seconds(total_pause_seconds);
                set_pause_seconds(pause_seconds);
        }

        play_song(found);
}

void stop(void)
{
        sound_system_stop(sound_sys);

        if (sound_system_get_state(sound_sys) == SOUND_STATE_STOPPED) {
                skip_to_begginning_of_song();
                emit_string_property_changed("PlaybackStatus", "Stopped");
        }
}

void ops_toggle_pause(void)
{
        AppState *state = get_app_state();
        PlaybackState *ps = get_playback_state();

        if (sound_system_get_state(sound_sys) == SOUND_STATE_STOPPED) {
                reset_clock();
        }

        if (get_current_song() == NULL && sound_system_get_state(sound_sys) == SOUND_STATE_PAUSED) {
                return;
        }

        sound_system_toggle_pause(sound_sys);

        if (sound_system_get_state(sound_sys) == SOUND_STATE_PLAYING && should_start_playing()) {
                sound_system_set_end_of_list_reached(sound_sys, false);
        }

        if (state->currentView != TRACK_VIEW) {
                trigger_refresh();
        }

        if (sound_system_get_state(sound_sys) == SOUND_STATE_PAUSED) {
                emit_string_property_changed("PlaybackStatus", "Paused");
                update_pause_time();
        } else {
                if (ps->hasSilentlySwitched && !ps->skipping) {
                        set_total_pause_seconds(0);
                        prepare_if_skipped_silent();
                } else {
                        set_total_pause_seconds(get_total_pause_seconds() + get_pause_seconds());
                }
                emit_string_property_changed("PlaybackStatus", "Playing");
        }
}

void seek(int seconds)
{
        Node *current = get_current_song();
        if (current == NULL)
                return;

        if (sound_system_get_state(sound_sys) == SOUND_STATE_STOPPED)
                return;

        add_to_accumulated_seconds(seconds);
}
