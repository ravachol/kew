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

#include "data/song_loader.h"

#include "utils/utils.h"

void load_first_song(Node *song)
{
        AppState *state = get_app_state();
        PlaybackState *ps = get_playback_state();

        if (song == NULL)
                return;

        ps->loadingdata.state = state;
        ps->loadingdata.loadingFirstDecoder = true;
        load_song(song, &ps->loadingdata);

        int i = 0;
        while (!ps->loadedNextSong && i < 10000) {
                if (i != 0 && i % 1000 == 0 && state->uiSettings.uiEnabled)
                        printf(".");
                c_sleep(10);
                fflush(stdout);
                i++;
        }
}

void unload_song_a(void)
{
        PlaybackState *ps = get_playback_state();

        if (audio_data.pUserData->songdataADeleted == false) {
                audio_data.pUserData->songdataADeleted = true;
                unload_song_data(&(ps->loadingdata.songdataA));
                audio_data.pUserData->songdataA = NULL;
        }
}

void unload_song_b(void)
{
        PlaybackState *ps = get_playback_state();

        if (audio_data.pUserData->songdataBDeleted == false) {
                audio_data.pUserData->songdataBDeleted = true;
                unload_song_data(&(ps->loadingdata.songdataB));
                audio_data.pUserData->songdataB = NULL;
        }
}

void unload_previous_song(void)
{
        AppState *state = get_app_state();
        UserData *user_data = audio_data.pUserData;
        PlaybackState *ps = get_playback_state();

        pthread_mutex_lock(&(ps->loadingdata.mutex));
        pthread_mutex_lock(&(state->data_source_mutex));

        if (ps->usingSongDataA &&
            (ps->skipping || (user_data->current_song_data == NULL ||
                              user_data->songdataADeleted == false ||
                              (ps->loadingdata.songdataA != NULL &&
                               user_data->songdataADeleted == false &&
                               user_data->current_song_data->hasErrors == 0 &&
                               user_data->current_song_data->track_id != NULL &&
                               strcmp(ps->loadingdata.songdataA->track_id,
                                      user_data->current_song_data->track_id) != 0)))) {
                unload_song_a();

                if (!audio_data.end_of_list_reached)
                        ps->loadedNextSong = false;

                ps->usingSongDataA = false;
        } else if (!ps->usingSongDataA &&
                   (ps->skipping ||
                    (user_data->current_song_data == NULL ||
                     user_data->songdataBDeleted == false ||
                     (ps->loadingdata.songdataB != NULL &&
                      user_data->songdataBDeleted == false &&
                      user_data->current_song_data->hasErrors == 0 &&
                      user_data->current_song_data->track_id != NULL &&
                      strcmp(ps->loadingdata.songdataB->track_id,
                             user_data->current_song_data->track_id) != 0)))) {
                unload_song_b();

                if (!audio_data.end_of_list_reached)
                        ps->loadedNextSong = false;

                ps->usingSongDataA = true;
        }

        pthread_mutex_unlock(&(state->data_source_mutex));
        pthread_mutex_unlock(&(ps->loadingdata.mutex));
}

int load_first(Node *song)
{
        load_first_song(song);
        Node *current = get_current_song();
        PlaybackState *ps = get_playback_state();

        ps->usingSongDataA = true;

        while (ps->songHasErrors && current->next != NULL) {
                ps->songHasErrors = false;
                ps->loadedNextSong = false;
                current = current->next;
                load_first_song(current);
        }

        if (ps->songHasErrors) {
                // Couldn't play any of the songs
                unload_previous_song();
                current = NULL;
                ps->songHasErrors = false;
                return -1;
        }

        UserData *user_data = audio_data.pUserData;

        user_data->currentPCMFrame = 0;
        user_data->current_song_data = user_data->songdataA;

        return 0;
}

void load_next_song(void)
{
        AppState *state = get_app_state();
        PlaybackState *ps = get_playback_state();

        ps->songLoading = true;
        ps->nextSongNeedsRebuilding = false;
        ps->skipFromStopped = false;
        ps->loadingdata.loadA = !ps->usingSongDataA;
        set_try_next_song(get_list_next(get_current_song()));
        set_next_song(get_try_next_song());
        ps->loadingdata.state = state;
        ps->loadingdata.loadingFirstDecoder = false;
        load_song(get_next_song(), &ps->loadingdata);
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
        audio_data.end_of_list_reached = false;
        set_song_to_start_from(find_path_in_playlist(path, playlist));
        ps->lastPlayedId = -1;
}
