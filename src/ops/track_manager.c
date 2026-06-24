/**
 * @file track_manager.c
 * @brief Central track indexing and metadata manager.
 *
 * Manages track objects, metadata lookup, and synchronization
 * between the library and active playlists. Provides helper
 * functions for finding and referencing track data.
 */

#include "track_manager.h"

#include "common/appstate.h"
#include "common/common.h"

#include "common/model.h"
#include "playback_clock.h"
#include "playback_ops.h"
#include "playback_state.h"
#include "playback_system.h"
#include "playlist_ops.h"

#include "sound/sound_facade.h"

#include "sys/mpris.h"
#include "sys/sys_integration.h"

#include "ui/common_ui.h"
#include "ui/components.h"

#include "utils/utils.h"

#include <stdbool.h>

typedef enum {
    LOAD_OK,
    LOAD_BAD_FILE
} SongLoadResult;

typedef enum {
    PLAYBACK_EVENT_EOF,
    PLAYBACK_EVENT_METADATA_SWITCH,
    PLAYBACK_EVENT_LOAD_SUCCESS,
    PLAYBACK_EVENT_LOAD_FAILED
} PlaybackEvent;

void load_song(Node *song, bool is_first_decoder, bool replace_next_song)
{
        PlaybackState *ps = get_playback_state();

        if (song == NULL) {
                ps->loadedNextSong = true;
                ps->skipping = false;
                ps->songLoading = false;
                return;
        }

        sound_result_t sound_result = sound_system_load(sound_sys, song->song.file_path, is_first_decoder, replace_next_song);

        if (sound_result == SOUND_ERROR_SONG)
                ps->songHasErrors = true;
}

void load_next_song(bool replace_next_song)
{
        PlaybackState *ps = get_playback_state();

        ps->songLoading = true;
        ps->nextSongNeedsRebuilding = false;
        ps->skipFromStopped = false;

        set_try_next_song(get_list_next(get_current_song()));
        set_next_song(get_try_next_song());
        load_song(get_next_song(), false, replace_next_song);
}

int load_first(Node *song)
{
        uninit_device();
        load_song(song, true, false);
        Node *current = get_current_song();
        PlaybackState *ps = get_playback_state();

        while (ps->songHasErrors && current->next != NULL) {
                ps->songHasErrors = false;
                ps->loadedNextSong = false;
                current = current->next;
                load_song(song, true, false);
        }

        if (ps->songHasErrors) {
                // Couldn't play any of the songs
                sound_system_unload_songs(sound_sys);
                current = NULL;
                ps->songHasErrors = false;
                return -1;
        }

        return 0;
}

void finish_loading(void)
{
        PlaybackState *ps = get_playback_state();

        int max_num_tries = 20;
        int numtries = 0;

        while (!ps->loadedNextSong && numtries < max_num_tries) {
                c_sleep(100);
                numtries++;
        }

        ps->loadedNextSong = true;
}

void autostart_if_stopped(const char *path)
{
        if (path == NULL)
                return;

        PlayList *playlist = get_playlist();
        PlaybackState *ps = get_playback_state();

        ps->waitingForPlaylist = false;
        ps->waitingForNext = true;
        sound_system_set_end_of_list_reached(sound_sys, false);
        set_song_to_start_from(find_path_in_playlist(path, playlist));
        ps->lastPlayedId = -1;
}

void determine_song_and_notify(void)
{
        Model *model = get_model();
        AppState *state = &model->state;
        SongData *current_song_data = NULL;
        bool isDeleted = sound_system_is_current_song_deleted(sound_sys);
        Node *current = get_current_song();
        model->songdata = get_current_song_data(model->songdata);

        if (current_song_data && current) {
                current->song.duration = current_song_data->duration;
        }

        if (state->ui.lastNotifiedId != current->id) {
                if (!isDeleted) {
                        notify_song_switch(current_song_data);
                }
        } else
                get_playback_state()->notifySwitch = true;
}

void preload_next_song(void)
{
        load_next_song(true);
        determine_song_and_notify();
}

void set_end_of_list_reached(void)
{
        Model *model = get_model();
        PlaybackState *ps = get_playback_state();

        ps->skipping = false;
        ps->skipOutOfOrder = false;

        sound_system_set_end_of_list_reached(sound_sys, true);

        if (!is_paused())
                start_playing(true);

        clear_current_song();

        uninit_device();

        set_dirty(DIRTY_ALL);

        if (is_repeat_list_enabled())
                repeat_list();
        else {
                emit_playback_stopped();
                emit_metadata_changed("", "", "", "",
                                      "/org/mpris/MediaPlayer2/TrackList/NoTrack",
                                      NULL, 0);
                if (model->state.currentView == TRACK_VIEW)
                        switch_view(LIBRARY_VIEW);
        }
}

void try_load_next(bool is_first_decoder, bool replace_next_song)
{
        PlaybackState *ps = get_playback_state();
        Node *current = get_current_song();
        Node *try_next_song = get_try_next_song();

        ps->songHasErrors = false;
        ps->clearingErrors = true;

        if (try_next_song == NULL && current != NULL) {
                try_next_song = current->next;
                set_try_next_song(try_next_song);
        } else if (try_next_song != NULL) {
                try_next_song = try_next_song->next;
                set_try_next_song(try_next_song);
        }
        if (try_next_song != NULL) {
                ps->songLoading = true;
                load_song(try_next_song, is_first_decoder, replace_next_song);
        } else {
                ps->clearingErrors = false;
        }
}

/**
 * @brief Prepares and plays the specified song.
 *
 * This function sets up the player to play the specified song, unloads previous songs,
 * and loads the new song. It then resumes playback, creating a playback device if needed.
 *
 * @param song Pointer to the song (Node) to be played.
 * @param seconds How far into the song to play.
 *
 * @return int Status of the operation: 0 on success, negative value on error.
 */
int prepare_and_play_song(Node *song, double seconds)
{
        if (!song)
                return -1;

        set_current_song(song);

        stop();

        int res = load_first(get_current_song());

        finish_loading();

        PlaybackState *ps = get_playback_state();
        while (ps->songHasErrors) {
                try_load_next(true, false);
                finish_loading();
        }

        if (res >= 0 && !ps->songHasErrors) {
                set_try_next_song(NULL);
                resume_playback(seconds);
        }

        if (res < 0)
                set_end_of_list_reached();

        reset_clock();

        return res;
}

void check_and_load_next_song(double seconds)
{
        AppState *state = get_app_state();
        PlaybackState *ps = get_playback_state();

        if (should_start_playing()) {
                PlayList *playlist = get_playlist();
                if (playlist->head == NULL)
                        return;

                if (ps->waitingForPlaylist || ps->waitingForNext) {
                        ps->songLoading = true;

                        Node *next_song = determine_next_song(playlist);
                        if (!next_song)
                                return;

                        start_playing(false);
                        ps->waitingForPlaylist = false;
                        ps->waitingForNext = false;
                        state->ui.songWasRemoved = false;

                        if (is_shuffle_enabled())
                                reshuffle_playlist();

                        int res = prepare_and_play_song(next_song, seconds);

                        if (res < 0)
                                set_end_of_list_reached();

                        ps->loadedNextSong = false;
                        set_next_song(NULL);
                }
        } else if (get_current_song() != NULL &&
                   (ps->nextSongNeedsRebuilding || get_next_song() == NULL) &&
                   !ps->songLoading) {
                preload_next_song();
        }
}

void handle_decoder_switch(void)
{
        // Finish loading before switching
        finish_loading();

        Model *model = get_model();
        Node *node = NULL;
        SongData *songdata = NULL;

        // Depending on if metadata has switched we have to get the current song or if it hasn't the next one
        if (model->state.ui.metadata_switched) {
                model->state.ui.metadata_switched = false;
                model->state.ui.decoder_switched = false;

                songdata = sound_system_get_current_song(sound_sys);
        } else {
                if (is_metadata_switch_reached())
                        songdata = sound_system_get_current_song(sound_sys);
                else
                        node = choose_next_song();
                model->state.ui.decoder_switched = true;
        }

        // Tell the system to load a next song
        if (!is_repeat_enabled() || (is_repeat_enabled() && is_paused()) || !node) {

                if (!sound_system_is_end_of_list_reached(sound_sys)) {
                        PlaybackState *ps = get_playback_state();
                        ps->loadedNextSong = false;
                }
        }

        char *file_path = songdata ? songdata->file_path : node ? node->song.file_path
                                                                : NULL;
        sound_system_switch_decoder(sound_sys, file_path);
}

void handle_metadata_switch(void)
{

        Model *model = get_model();
        AppState *state = &model->state;

        move_to_next_song_in_the_playlist();

        if (model->state.ui.decoder_switched) {
                model->state.ui.decoder_switched = false;
                model->state.ui.metadata_switched = false;
        } else {
                model->state.ui.metadata_switched = true;
        }

        Node *current = get_current_song();

        if (current == NULL) {

                if (!sound_system_is_end_of_list_reached(sound_sys)) {
                        PlaybackState *ps = get_playback_state();
                        ps->loadedNextSong = false;
                }
        }

        if (current == NULL) {
                if (state->settings.quitAfterStopping) {
                        quit();
                } else {
                        set_end_of_list_reached();
                }
        } else {
                determine_song_and_notify();
        }

        // Tell the system there's no next song, as we just switched to the one that was the next
        set_next_song(NULL);

        int enter_song_at_ms = 0;
        sound_system_get_fade_started(sound_sys, &enter_song_at_ms);

        reset_clock();
        clock_add_offset(-enter_song_at_ms);

        component_playlist_helper_update_view_state(model);
}

void load_waiting_music(void)
{
        PlayList *playlist = get_playlist();
        PlaybackState *ps = get_playback_state();

        if (playlist->head != NULL) {
                if ((ps->skipFromStopped || !ps->loadedNextSong ||
                     ps->nextSongNeedsRebuilding) &&
                    !sound_system_is_end_of_list_reached(sound_sys)) {
                        check_and_load_next_song(0.0);
                }

                if (ps->songHasErrors)
                        try_load_next(false, true);

                if (is_EOF_reached()) {
                        sound_system_set_EOF_switch(sound_sys, false);
                        handle_decoder_switch();
                }

                if (is_metadata_switch_reached()) {
                        sound_system_set_metadata_switch(sound_sys, false);
                        handle_metadata_switch();
                }

        } else {
                sound_system_set_EOF_switch(sound_sys, false);
        }
}
