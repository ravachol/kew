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

#include "playback_system.h"

#include "sound/sound_facade.h"

#include "utils/utils.h"

void load_song(Node *song, bool is_first_decoder, bool replace_next_song)
{
        PlaybackState *ps = get_playback_state();

        if (song == NULL) {
                ps->loadedNextSong = true;
                ps->skipping = false;
                ps->songLoading = false;
                return;
        }

        sound_system_load(sound_sys, song->song.file_path, is_first_decoder, replace_next_song);
}

void load_first_song(Node *song)
{
        AppState *state = get_app_state();
        PlaybackState *ps = get_playback_state();

        if (song == NULL)
                return;

        uninit_device();

        load_song(song, true, false);

        int i = 0;
        while (!ps->loadedNextSong && i < 10000) {
                if (i != 0 && i % 1000 == 0 && state->uiSettings.uiEnabled)
                        printf(".");
                c_sleep(10);
                fflush(stdout);
                i++;
        }
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
        load_first_song(song);
        Node *current = get_current_song();
        PlaybackState *ps = get_playback_state();

        while (ps->songHasErrors && current->next != NULL) {
                ps->songHasErrors = false;
                ps->loadedNextSong = false;
                current = current->next;
                load_first_song(current);
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
