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

#include "sound/decoders.h"
#ifdef USE_FAAD
#include "sound/m4a.h"
#endif
#include "sound/playback.h"

#include "sound/volume.h"
#include "sys/sys_integration.h"

#include "data/song_loader.h"

#include "utils/file.h"
#include "utils/utils.h"

void resume_playback(void)
{
        sound_resume_playback();
}

int load_decoder(SongData *song_data, bool *song_data_deleted)
{
        PlaybackState *ps = get_playback_state();
        int result = 0;

        if (song_data != NULL) {
                *song_data_deleted = false;

                // This should only be done for the second song, as
                // switch_audio_implementation() handles the first one
                if (!ps->loadingdata.loadingFirstDecoder) {
                        if (has_builtin_decoder(song_data->file_path))
                                result = prepare_next_decoder(song_data->file_path);
                        else if (path_ends_with(song_data->file_path, "opus"))
                                result =
                                    prepare_next_opus_decoder(song_data->file_path);
                        else if (path_ends_with(song_data->file_path, "ogg"))
                                result = prepare_next_vorbis_decoder(
                                    song_data->file_path);
                        else if (path_ends_with(song_data->file_path, "webm"))
                                result = prepare_next_webm_decoder(song_data);
#ifdef USE_FAAD
                        else if (path_ends_with(song_data->file_path, "m4a") ||
                                 path_ends_with(song_data->file_path, "aac"))
                                result = prepare_next_m4a_decoder(song_data);

#endif
                }
        }
        return result;
}

int assign_loaded_data(void)
{
        PlaybackState *ps = get_playback_state();
        int result = 0;

        if (ps->loadingdata.loadA) {
                audio_data.pUserData->songdataA = ps->loadingdata.songdataA;
                result = load_decoder(ps->loadingdata.songdataA,
                                      &(audio_data.pUserData->songdataADeleted));
        } else {
                audio_data.pUserData->songdataB = ps->loadingdata.songdataB;
                result = load_decoder(ps->loadingdata.songdataB,
                                      &(audio_data.pUserData->songdataBDeleted));
        }

        return result;
}

void *song_data_reader_thread(void *arg)
{
        PlaybackState *ps = (PlaybackState *)arg;

        pthread_mutex_lock(&(ps->loadingdata.mutex));

        char filepath[PATH_MAX];
        c_strcpy(filepath, ps->loadingdata.file_path, sizeof(filepath));

        SongData *songdata = exists_file(filepath) >= 0 ? load_song_data(filepath) : NULL;

        if (ps->loadingdata.loadA) {
                if (!audio_data.pUserData->songdataADeleted) {
                        unload_song_data(&(ps->loadingdata.songdataA));
                }
                audio_data.pUserData->songdataADeleted = false;
                audio_data.pUserData->songdataA = songdata;
                ps->loadingdata.songdataA = songdata;
        } else {
                if (!audio_data.pUserData->songdataBDeleted) {
                        unload_song_data(&(ps->loadingdata.songdataB));
                }
                audio_data.pUserData->songdataBDeleted = false;
                audio_data.pUserData->songdataB = songdata;
                ps->loadingdata.songdataB = songdata;
        }

        int result = assign_loaded_data();

        if (result < 0)
                songdata->hasErrors = true;

        pthread_mutex_unlock(&(ps->loadingdata.mutex));

        if (songdata == NULL || songdata->hasErrors) {
                ps->songHasErrors = true;
                ps->clearingErrors = true;
                set_next_song(NULL);
        } else {
                ps->songHasErrors = false;
                ps->clearingErrors = false;
                set_next_song(get_try_next_song());
                set_try_next_song(NULL);
        }

        ps->loadedNextSong = true;
        ps->skipping = false;
        ps->songLoading = false;

        return NULL;
}

void load_song(Node *song, LoadingThreadData *loadingdata)
{
        PlaybackState *ps = get_playback_state();

        if (song == NULL) {
                ps->loadedNextSong = true;
                ps->skipping = false;
                ps->songLoading = false;
                return;
        }

        c_strcpy(loadingdata->file_path, song->song.file_path,
                 sizeof(loadingdata->file_path));

        pthread_t loading_thread;
        pthread_create(&loading_thread, NULL, song_data_reader_thread, ps);
}

void try_load_next(void)
{
        AppState *state = get_app_state();
        PlaybackState *ps = get_playback_state();
        Node *current = get_current_song();
        Node *try_next_song = get_try_next_song();

        ps->songHasErrors = false;
        ps->clearingErrors = true;

        if (try_next_song == NULL && current != NULL)
                try_next_song = current->next;
        else if (try_next_song != NULL)
                try_next_song = try_next_song->next;

        if (try_next_song != NULL) {
                ps->songLoading = true;
                ps->loadingdata.state = state;
                ps->loadingdata.loadA = !ps->usingSongDataA;
                ps->loadingdata.loadingFirstDecoder = false;
                load_song(try_next_song, &ps->loadingdata);
        } else {
                ps->clearingErrors = false;
        }
}

void pause_song(void)
{
        if (!pb_is_paused()) {
                emit_string_property_changed("PlaybackStatus", "Paused");
                update_pause_time();
        }
        pause_playback();
}

void skip_to_begginning_of_song(void)
{
        reset_clock();

        if (get_current_song() != NULL) {
                seek_percentage(0);
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
                set_current_implementation_type(NONE);
                set_repeat_enabled(false);

                audio_data.end_of_list_reached = false;

                ps->usingSongDataA = !ps->usingSongDataA;

                ps->skipping = false;
        }
}

void play(void)
{
        PlaybackState *ps = get_playback_state();

        if (pb_is_paused()) {
                set_total_pause_seconds(get_total_pause_seconds() + get_pause_seconds());
                emit_string_property_changed("PlaybackStatus", "Playing");
        } else if (pb_is_stopped()) {
                emit_string_property_changed("PlaybackStatus", "Playing");
        }

        if (pb_is_stopped() && !ps->hasSilentlySwitched) {
                skip_to_begginning_of_song();
        }

        sound_resume_playback();

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
        AppState *state = get_app_state();
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
        if (ps->waitingForPlaylist || audio_data.restart) {
                ps->waitingForPlaylist = false;
                audio_data.restart = false;

                if (is_shuffle_enabled())
                        reshuffle_playlist();
        }

        ps->loadingdata.state = state;
        ps->loadingdata.loadA = !ps->usingSongDataA;
        ps->loadingdata.loadingFirstDecoder = true;

        load_song(node, &ps->loadingdata);

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
        adjust_volume_percent(change_percent);
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

void stop_at_shutdown(void)
{
        stop_playback();
}

void stop(void)
{
        stop_playback();

        if (pb_is_stopped()) {
                skip_to_begginning_of_song();
                emit_string_property_changed("PlaybackStatus", "Stopped");
        }
}

void ops_toggle_pause(void)
{
        PlaybackState *ps = get_playback_state();

        if (pb_is_stopped()) {
                reset_clock();
        }

        if (get_current_song() == NULL && pb_is_paused()) {
                return;
        }

        toggle_pause_playback();

        if (pb_is_paused()) {
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

#ifdef USE_FAAD
        if (path_ends_with(current->song.file_path, "aac")) {
                m4a_decoder *decoder = get_current_m4a_decoder();
                if (decoder != NULL && decoder->file_type == k_rawAAC)
                        return;
        }
#endif

        if (pb_is_paused())
                return;

        double duration = current->song.duration;
        if (duration <= 0.0)
                return;

        add_to_accumulated_seconds(seconds);
}
