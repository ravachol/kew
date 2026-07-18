/**
 * @file playlist_ops.c
 * @brief Playlist management operations.
 *
 * Implements functionality for adding, removing, reordering,
 * shuffling, and retrieving songs within playlists.
 * Coordinates with the playback system for next/previous transitions.
 */

#include "playlist_ops.h"

#include "common/common.h"
#include "common/model.h"
#include "library_ops.h"
#include "playback_clock.h"
#include "playback_ops.h"
#include "playback_state.h"
#include "playback_system.h"

#include "library_ops.h"
#include "sound/audiotypes.h"
#include "sys/mpris.h"
#include "track_manager.h"

#include "common/appstate.h"

#include "sound/sound_facade.h"

#include "sys/sys_integration.h"

#include "ui/components.h"
#include "utils/file.h"
#include "utils/utils.h"

static bool skip_in_progress = false;
static int num_playlist_name_letters = 0;
static int num_playlist_name_bytes = 0;
static int min_playlist_name_letters = 1;

Node *choose_next_song(void)
{
        Node *current = get_current_song();
        Node *next_song = get_next_song();

        if (next_song != NULL)
                return next_song;
        else if (current != NULL && current->next != NULL) {
                return current->next;
        } else {
                return NULL;
        }
}

Node *find_selected_entry_by_id(PlayList *playlist, int id)
{
        Node *node = playlist->head;

        if (node == NULL || id < 0)
                return NULL;

        bool found = false;

        for (int i = 0; i < playlist->count; i++) {
                if (node != NULL && node->id == id) {
                        found = true;
                        break;
                } else if (node == NULL) {
                        return NULL;
                }
                node = node->next;
        }

        if (found) {
                return node;
        }

        return NULL;
}

Node *find_selected_entry(PlayList *playlist, int row)
{
        Node *node = playlist->head;

        if (node == NULL)
                return NULL;

        bool found = false;

        for (int i = 0; i < playlist->count; i++) {
                if (i == row) {
                        found = true;
                        break;
                }
                node = node->next;
        }

        if (found) {
                return node;
        }

        return NULL;
}

void stop_and_clear_current_song(void)
{
        Node *current = get_current_song();
        if (current == NULL)
                return;

        sound_system_stop(sound_sys);
        emit_playback_stopped();
        sound_system_clear_current_track(sound_sys);
        clear_current_song();
        set_dirty(DIRTY_SONG);
}

void remove_currently_playing_song(void)
{
        Node *current = get_current_song();
        PlaybackState *ps = get_playback_state();

        stop_and_clear_current_song();

        ps->loadedNextSong = false;
        start_playing(true);
        sound_system_set_end_of_list_reached(sound_sys, true);

        if (current != NULL) {
                ps->lastPlayedId = current->id;
                set_song_to_start_from(get_list_next(current));
        }
        ps->waitingForNext = true;
        current = NULL;
}

void rebuild_next_song(Node *song)
{
        if (song == NULL)
                return;

        PlaybackState *ps = get_playback_state();
        ps->songLoading = true;

        load_song(song, false, true);

        int max_num_tries = 50;
        int numtries = 0;

        while (ps->songLoading && !ps->loadedNextSong && numtries < max_num_tries) {
                c_sleep(100);
                numtries++;
        }
        ps->songLoading = false;
}

void add_to_favorites_playlist(void)
{
        Node *current = get_current_song();
        PlayList *favorites_playlist = get_favorites_playlist();

        if (current == NULL)
                return;

        int id = current->id;

        Node *node = NULL;

        if (find_selected_entry_by_id(favorites_playlist, id) !=
            NULL) // Song is already in list
                return;

        create_node(&node, current->song.file_path, id);
        add_to_list(favorites_playlist, node);
}

// Go through the display playlist and the shuffle playlist to remove all songs
void clear_playlist(void)
{
        bool clearAll = false;
        int current_id = -1;

        Node *current = get_current_song();
        PlayList *unshuffled_playlist = get_unshuffled_playlist();
        PlayList *playlist = get_playlist();
        AppState *state = get_app_state();

        // If option is enabled, stop playback so clearAll becomes true.
        if (state->settings.clearListClearsAll) {
                stop_and_clear_current_song();
                current = NULL;
        }

        // Do we need to clear the entire playlist?
        if (current == NULL) {
                clearAll = true;
        } else {
                current_id = current->id;
        }

        int next_in_playlist_id;
        pthread_mutex_lock(&(playlist->mutex));
        Node *song_to_be_removed;
        Node *next_in_playlist = unshuffled_playlist->head;

        while (next_in_playlist != NULL) {
                next_in_playlist_id = next_in_playlist->id;

                if (clearAll || next_in_playlist_id != current_id) {
                        song_to_be_removed = next_in_playlist;

                        next_in_playlist = next_in_playlist->next;

                        int id = song_to_be_removed->id;

                        // Update Library
                        if (song_to_be_removed != NULL)
                                mark_as_dequeued(get_library(),
                                                 song_to_be_removed->song.file_path);

                        // Remove from Display playlist
                        if (song_to_be_removed != NULL)
                                delete_from_list(unshuffled_playlist,
                                                 song_to_be_removed);

                        // Remove from Shuffle playlist
                        Node *node2 = find_selected_entry_by_id(playlist, id);
                        if (node2 != NULL)
                                delete_from_list(playlist, node2);
                } else {
                        next_in_playlist = next_in_playlist->next;
                }
        }
        clear_all_m3u_enqueued_flags(get_library());
        pthread_mutex_unlock(&(playlist->mutex));

        PlaybackState *ps = get_playback_state();
        ps->nextSongNeedsRebuilding = true;
        set_next_song(NULL);

        if (state->settings.clearListClearsAll)
                start_playing(true);

        // Only refresh the screen if it makes sense to do so
        if (state->currentView == PLAYLIST_VIEW ||
            state->currentView == LIBRARY_VIEW ||
            state->settings.clearListClearsAll) {
                set_dirty(DIRTY_ALL);
        }
}

Node *get_song_by_number(PlayList *playlist, int song_number)
{
        Node *song = playlist->head;

        if (!song)
                return get_current_song();

        if (song_number <= 0) {
                return song;
        }

        int count = 1;

        while (song->next != NULL && count != song_number) {
                song = get_list_next(song);
                count++;
        }

        return song;
}

void set_current_song_to_next(void)
{
        Node *current = get_current_song();
        PlaybackState *ps = get_playback_state();

        if (current != NULL)
                ps->lastPlayedId = current->id;

        set_current_song(choose_next_song());
}

void set_current_song_to_prev(void)
{
        Node *current = get_current_song();
        PlaybackState *ps = get_playback_state();

        if (current != NULL && current->prev != NULL) {
                ps->lastPlayedId = current->id;
                set_current_song(current->prev);
        }
}

void silent_switch_to_next(bool load_song)
{
        PlaybackState *ps = get_playback_state();

        set_next_song(NULL);

        if (choose_next_song() != NULL) {
                ps->skipping = true;
                set_current_song_to_next();
        } else
                return;

        ps->skipOutOfOrder = true;

        if (load_song) {
                load_next_song(false);
                finish_loading();
                ps->loadedNextSong = true;
                ps->notifySwitch = true;
        }

        reset_clock();

        sound_system_switch_song_immediate(sound_sys);

        set_dirty(DIRTY_ALL);

        ps->skipping = false;
        ps->hasSilentlySwitched = true;
        ps->nextSongNeedsRebuilding = false;

        pause_song();

        set_next_song(NULL);
}

void silent_switch_to_prev(void)
{
        PlaybackState *ps = get_playback_state();

        ps->skipping = true;

        set_current_song_to_prev();
        sound_system_switch_song_immediate(sound_sys);

        ps->loadedNextSong = false;
        ps->songLoading = true;
        ps->forceSkip = false;

        load_song(get_current_song(), true, false);
        finish_loading();
        reset_clock();

        set_dirty(DIRTY_ALL);

        ps->skipping = false;
        ps->nextSongNeedsRebuilding = true;

        pause_song();

        set_next_song(NULL);

        ps->notifySwitch = true;
        ps->skipOutOfOrder = true;
        ps->hasSilentlySwitched = true;
}

void switch_to_next_song(void)
{
        Model *model = get_model();

        Node *current = get_current_song();
        Node *next = current ? current->next : NULL;

        if (model->state.settings.repeatState == SOUND_STATE_REPEAT &&
            (model->playlist->count == 1 || !next)) {
                skip_to_begginning_of_song();
                return;
        }

        PlaybackState *ps = get_playback_state();

        // Stop if there is no song or no next song
        if ((current == NULL && !is_paused()) || next == NULL) {
                if (is_repeat_list_enabled()) {
                        clear_current_song();
                } else if (sound_system_get_state(sound_sys) == SOUND_STATE_PLAYING ||
                           sound_system_get_state(sound_sys) == SOUND_STATE_PAUSED) {
                        stop();
                        set_dirty(DIRTY_ALL);
                        return;
                } else {
                        set_dirty(DIRTY_VISUALIZER);
                        return;
                }
        }

        if (ps->songLoading || ps->nextSongNeedsRebuilding || ps->skipping ||
            ps->clearingErrors)
                return;

        if (sound_system_get_state(sound_sys) != SOUND_STATE_PLAYING) {

                if (!(next == NULL && model->state.settings.repeatState == SOUND_STATE_REPEAT_LIST)) {
                        silent_switch_to_next(true);
                        return;
                } else {
                        set_end_of_list_reached();
                        return;
                }
        }

        if (is_shuffle_enabled())
                component_playlist_helper_update_view_state(model, false);

        double total_pause_seconds = get_total_pause_seconds();
        double pause_seconds = get_total_pause_seconds();

        play();

        set_total_pause_seconds(total_pause_seconds);
        set_pause_seconds(pause_seconds);

        ps->skipping = true;
        ps->skipOutOfOrder = false;

        reset_clock();

        skip();
}

void switch_to_prev_song(void)
{

        if (skip_in_progress)
                return;

        Model *model = get_model();

        if (model->state.settings.repeatState == SOUND_STATE_REPEAT && model->playlist->count == 1) {
                skip_to_begginning_of_song();
                return;
        }

        skip_in_progress = true;
        int retries = 0;
retry: {
        Node *current = get_current_song();
        PlaybackState *ps = get_playback_state();

        if (current == NULL) {

                if (sound_system_get_state(sound_sys) == SOUND_STATE_PLAYING)
                        stop();

                skip_in_progress = false;
                return;
        }

        if (ps->songLoading || ps->skipping || ps->clearingErrors)
                if (!ps->forceSkip) {
                        skip_in_progress = false;
                        return;
                }

        if (sound_system_get_state(sound_sys) != SOUND_STATE_PLAYING) {
                silent_switch_to_prev();
                skip_in_progress = false;
                return;
        }

        Node *song = current;

        set_current_song_to_prev();

        if (song == get_current_song() && retries == 0) {
                reset_clock();
                update_playback_position(
                    0); // We need to signal to mpris that the song was
                        // reset to the beginning
        }

        double total_pause_seconds = get_total_pause_seconds();
        double pause_seconds = get_total_pause_seconds();

        set_total_pause_seconds(total_pause_seconds);
        set_pause_seconds(pause_seconds);

        ps->skipping = true;
        ps->skipOutOfOrder = true;
        ps->songLoading = true;
        ps->forceSkip = false;
        ps->loadedNextSong = false;

        load_song(get_current_song(), false, true);

        if (ps->songHasErrors) {
                 if (!(song == get_current_song() && retries > 0)) {
                        ps->songHasErrors = false;
                        ps->forceSkip = true;
                        retries++;

                        goto retry;
                }
                else {
                        ps->skipping = false;
                        ps->skipOutOfOrder = false;
                        ps->songLoading = false;
                        ps->forceSkip = false;
                        ps->loadedNextSong = false;
                        ps->songHasErrors = false;
                        skip_in_progress = false;
                        set_next_song(NULL);
                        set_current_song_to_next();
                        clear_and_play(get_current_song());
                        return;
                }
        }

        play();

        reset_clock();

        skip();

        skip_in_progress = false;
}
}

void delete_from_lists(Node *node)
{
        PlayList *playlist = get_playlist();
        PlayList *unshuffled_playlist = get_unshuffled_playlist();

        if (node != NULL)
                mark_as_dequeued(get_library(), node->song.file_path);

        Node *node2 = find_selected_entry_by_id(playlist, node->id);

        if (node != NULL)
                delete_from_list(unshuffled_playlist, node);

        if (node2 != NULL)
                delete_from_list(playlist, node2);
}

void remove_song(Node *node)
{
        PlayList *playlist = get_playlist();
        Node *current = get_current_song();
        bool rebuild = false;

        if (node == NULL) {
                return;
        }

        Node *song = choose_next_song();
        int id = node->id;
        int current_id = (current != NULL) ? current->id : -1;

        if (current_id == node->id) {
                remove_currently_playing_song();
        } else {
                if (get_song_to_start_from() != NULL) {
                        set_song_to_start_from(get_list_next(node));
                }
        }

        PlaybackState *ps = get_playback_state();

        pthread_mutex_lock(&(playlist->mutex));

        if (node != NULL && song != NULL && current != NULL) {
                if (strcmp(song->song.file_path, node->song.file_path) ==
                        0 ||
                    (current != NULL && current->next != NULL &&
                     id == current->next->id))
                        rebuild = true;
        }

        delete_from_lists(node);

        if (is_shuffle_enabled())
                rebuild = true;

        current = find_selected_entry_by_id(playlist, current_id);

        if (rebuild && current != NULL) {
                node = NULL;
                set_next_song(NULL);
                reshuffle_playlist();

                set_try_next_song(current->next);

                ps->nextSongNeedsRebuilding = false;
                set_next_song(NULL);
                set_next_song(get_list_next(current));
                rebuild_next_song(get_next_song());
                ps->loadedNextSong = true;
        }

        pthread_mutex_unlock(&(playlist->mutex));

        if (playlist->head == NULL)
                ps->waitingForPlaylist = true;
}

void handle_remove(int chosen_row)
{
        Model *model = get_model();
        PlayList *unshuffled_playlist = get_unshuffled_playlist();
        Node *node = NULL;

        if (model->state.currentView == PLAYLIST_VIEW) {
                Node *current = get_current_song();
                node = find_selected_entry(unshuffled_playlist, chosen_row);
                if (current && node && node->id == current->id) {
                        model->state.ui.resetPlaylistDisplay = true;
                        set_dirty(DIRTY_ALL);
                } else
                        set_dirty(DIRTY_PLAYLIST);

                remove_song(node);
        } else {
                Node *current = get_current_song();
                if (current)
                        node = find_selected_entry_by_id(unshuffled_playlist, current->id);

                if (is_paused() && current->id == node->id) {
                        model->state.ui.resetPlaylistDisplay = true;
                        switch_to_next_song();
                }

                remove_song(node);

                Node *next = get_next_song();
                if (next) {
                        clear_and_play(next);
                }
        }
}

void skip_to_numbered_song(int song_number)
{
        PlayList *playlist = get_playlist();
        PlaybackState *ps = get_playback_state();

        if (ps->songLoading || ps->skipping || ps->clearingErrors)
                if (!ps->forceSkip)
                        return;

        double total_pause_seconds = get_total_pause_seconds();
        double pause_seconds = get_total_pause_seconds();

        set_total_pause_seconds(total_pause_seconds);
        set_pause_seconds(pause_seconds);

        ps->skipping = true;
        ps->skipOutOfOrder = true;
        ps->loadedNextSong = false;
        ps->songLoading = true;
        ps->forceSkip = false;

        set_current_song(get_song_by_number(playlist, song_number));

        sound_system_stop(sound_sys);

        load_song(get_current_song(), true, false);

        int max_num_tries = 50;
        int numtries = 0;

        while (!ps->loadedNextSong && numtries < max_num_tries) {
                c_sleep(100);
                numtries++;
        }

        if (ps->songHasErrors) {
                ps->songHasErrors = false;
                ps->forceSkip = true;
                if (song_number < playlist->count)
                        skip_to_numbered_song(song_number + 1);
        }

        reset_clock();

        skip();
}

void skip_to_last_song(void)
{
        PlayList *playlist = get_playlist();

        Node *song = playlist->head;

        if (!song)
                return;

        int count = 1;
        while (song->next != NULL) {
                song = get_list_next(song);
                count++;
        }
        skip_to_numbered_song(count);
}

void repeat_list(void)
{
        sound_system_set_end_of_list_reached(sound_sys, false);
        skip_to_numbered_song(1);
        start_playing(false);
        PlaybackState *ps = get_playback_state();
        ps->skipOutOfOrder = false;
}

void move_song_up(int *chosen_row)
{
        AppState *state = get_app_state();
        PlayList *unshuffled_playlist = get_unshuffled_playlist();
        PlayList *playlist = get_playlist();
        PlaybackState *ps = get_playback_state();

        if (state->currentView != PLAYLIST_VIEW) {
                return;
        }

        bool rebuild = false;

        Node *node = find_selected_entry(unshuffled_playlist, *chosen_row);

        if (node == NULL) {
                return;
        }

        int id = node->id;

        pthread_mutex_lock(&(playlist->mutex));

        Node *current = get_current_song();

        if (node != NULL && current != NULL) {
                // Rebuild if current song, the next song or the song after are
                // affected
                if (current != NULL) {
                        Node *tmp = current;

                        for (int i = 0; i < 3; i++) {
                                if (tmp == NULL)
                                        break;

                                if (tmp->id == id) {
                                        rebuild = true;
                                }
                                tmp = tmp->next;
                        }
                }
        }

        move_up_list(unshuffled_playlist, node, true);
        Node *pl_node = find_selected_entry_by_id(playlist, node->id);

        if (!is_shuffle_enabled())
                move_up_list(playlist, pl_node, false);

        *chosen_row = *chosen_row - 1;
        *chosen_row = (*chosen_row > 0) ? *chosen_row : 0;

        if (rebuild && current != NULL) {
                node = NULL;
                ps->nextSongNeedsRebuilding = false;

                set_try_next_song(current->next);
                set_next_song(get_list_next(current));
                rebuild_next_song(get_next_song());

                ps->loadedNextSong = true;
        }

        pthread_mutex_unlock(&(playlist->mutex));

        set_dirty(DIRTY_PLAYLIST);
}

void move_song_down(int *chosen_row)
{
        AppState *state = get_app_state();
        PlayList *playlist = get_playlist();
        PlayList *unshuffled_playlist = get_unshuffled_playlist();
        PlaybackState *ps = get_playback_state();

        if (state->currentView != PLAYLIST_VIEW) {
                return;
        }

        bool rebuild = false;

        Node *node = find_selected_entry(unshuffled_playlist, *chosen_row);

        Node *current = get_current_song();

        if (node == NULL) {
                return;
        }

        int id = node->id;

        pthread_mutex_lock(&(playlist->mutex));

        if (node != NULL && current != NULL) {
                // Rebuild if current song, the next song or the previous song
                // are affected
                if (current != NULL) {
                        Node *tmp = current;

                        for (int i = 0; i < 2; i++) {
                                if (tmp == NULL)
                                        break;

                                if (tmp->id == id) {
                                        rebuild = true;
                                }
                                tmp = tmp->next;
                        }

                        if (current->prev != NULL && current->prev->id == id)
                                rebuild = true;
                }
        }

        move_down_list(unshuffled_playlist, node, true);

        Node *pl_node = find_selected_entry_by_id(playlist, node->id);

        if (!is_shuffle_enabled())
                move_down_list(playlist, pl_node, false);

        *chosen_row = *chosen_row + 1;
        *chosen_row = (*chosen_row >= playlist->count)
                          ? playlist->count - 1
                          : *chosen_row;

        if (rebuild && current != NULL) {
                node = NULL;
                ps->nextSongNeedsRebuilding = false;

                set_try_next_song(current->next);
                set_next_song(get_list_next(current));
                rebuild_next_song(get_next_song());
                ps->loadedNextSong = true;
        }

        pthread_mutex_unlock(&(playlist->mutex));

        set_dirty(DIRTY_PLAYLIST);
}

void reshuffle_playlist(void)
{
        PlayList *playlist = get_playlist();
        PlaybackState *ps = get_playback_state();

        if (is_shuffle_enabled()) {
                Node *current = get_current_song();

                if (current != NULL)
                        shuffle_playlist_starting_from_song(playlist, current);
                else
                        shuffle_playlist(playlist);

                ps->nextSongNeedsRebuilding = true;
        }
}

void move_to_next_song_in_the_playlist(void)
{
        PlaybackState *ps = get_playback_state();

        if (!ps->skipOutOfOrder && !is_repeat_enabled()) {
                set_current_song_to_next();
        } else {
                ps->skipOutOfOrder = false;
        }
}

Node *determine_next_song(PlayList *playlist)
{
        AppState *state = get_app_state();
        Node *current = NULL;
        PlaybackState *ps = get_playback_state();

        if (ps->waitingForPlaylist) {
                return playlist->head;
        } else if (ps->waitingForNext) {
                Node *song_to_start_from = get_song_to_start_from();

                if (song_to_start_from != NULL) {
                        find_node_in_list(playlist, song_to_start_from->id, &current);
                        return current ? current : NULL;
                } else if (ps->lastPlayedId >= 0) {
                        current = find_selected_entry_by_id(playlist, ps->lastPlayedId);
                        if (current != NULL && current->next != NULL)
                                current = current->next;

                        return current;
                }

                // fallback if nothing else
                if (state->ui.startFromTop) {
                        state->ui.startFromTop = false;
                        return playlist->head;
                } else {
                        return playlist->tail;
                }
        }

        return NULL;
}

bool play_pre_processing()
{
        bool was_end_of_list = false;

        if (sound_system_is_end_of_list_reached(sound_sys))
                was_end_of_list = true;

        return was_end_of_list;
}

void play_post_processing(bool was_end_of_list)
{
        AppState *state = get_app_state();
        PlaybackState *ps = get_playback_state();

        if ((state->ui.songWasRemoved && get_current_song() != NULL)) {
                state->ui.songWasRemoved = false;
        }

        if (was_end_of_list) {
                ps->skipOutOfOrder = false;
        }

        sound_system_set_end_of_list_reached(sound_sys, false);
}

void clear_and_play(Node *song)
{
        PlaybackState *ps = get_playback_state();
        set_song_to_start_from(song);
        sound_system_stop_decoding(sound_sys);
        start_playing(true);
        sound_system_set_end_of_list_reached(sound_sys, false);
        ps->nextSongNeedsRebuilding = true;
        ps->skipOutOfOrder = false;
        ps->waitingForNext = true;
        ps->loadedNextSong = false;
        ps->lastPlayedId = -1;
}

void playlist_play(PlayList *playlist)
{
        AppState *state = get_app_state();

        Node *current = get_current_song();

        if (is_paused() && current != NULL &&
            state->ui.chosen_node_id == current->id) {
                ops_toggle_pause();
        } else {
                Node *song = NULL;
                find_node_in_list(playlist,
                                  state->ui.chosen_node_id,
                                  &song);

                clear_and_play(song);
        }
}

void play_favorites_playlist(void)
{
        PlayList *playlist = get_playlist();
        PlayList *favorites_playlist = get_favorites_playlist();

        if (favorites_playlist->count == 0) {
                printf(_("Couldn't find any songs in the special playlist. Add a "
                         "song by pressing '.' while it's playing. \n"));
                quit();
        }

        FileSystemEntry *library = get_library();

        deep_copy_list(favorites_playlist, &playlist);
        shuffle_playlist(playlist);

        mark_list_as_enqueued(library, playlist);
}

void play_all(void)
{
        FileSystemEntry *library = get_library();
        PlayList *playlist = get_playlist();

        create_play_list_from_file_system_entry(library, playlist, MAX_FILES);

        if (playlist->count == 0) {
                quit();
        }

        shuffle_playlist(playlist);
        mark_list_as_enqueued(library, playlist);
}

void play_all_albums(void)
{
        PlayList *playlist = get_playlist();
        FileSystemEntry *library = get_library();
        add_shuffled_albums_to_play_list(library, playlist, MAX_FILES);

        if (playlist->count == 0) {
                quit();
        }

        mark_list_as_enqueued(library, playlist);
}

void play_command_with_playlist(int argc, char **argv)
{
        PlayList *playlist = get_playlist();
        const char *allowed_extensions = MUSIC_FILE_EXTENSIONS;

        for (int i = 2; i < argc; i++) {
                build_playlist_recursive(argv[i], allowed_extensions, playlist);
        }
}

int add_to_playlist_name(const char *str)
{
        Model *model = get_model();

        if (str == NULL) {
                return -1;
        }

        size_t len = strnlen(str, MAX_PLAYLIST_NAME_LEN);

        // Check if the string can fit into the playlist name buffer
        if (num_playlist_name_letters + len > MAX_PLAYLIST_NAME_LEN) {
                return 0; // Not enough space
        }

        // Add the string to the playlist name text buffer
        for (size_t i = 0; i < len; i++) {
                model->state.ui.playlist_name[num_playlist_name_bytes++] = str[i];
        }

        model->state.ui.playlist_name[num_playlist_name_bytes] = '\0'; // Null-terminate the buffer

        num_playlist_name_letters++;

        set_dirty(DIRTY_VISUALIZER | DIRTY_FOOTER);

        return 0;
}

int remove_from_playlist_name(void)
{
        Model *model = get_model();

        if (num_playlist_name_letters == 0)
                return 0;

        // Determine the number of bytes to remove for the last character
        int last_char_bytes = get_last_char_bytes(model->state.ui.playlist_name, num_playlist_name_bytes);
        if (last_char_bytes == 0)
                return 0;

        // Remove the character from the buffer
        num_playlist_name_bytes -= last_char_bytes;
        model->state.ui.playlist_name[num_playlist_name_bytes] = '\0';

        num_playlist_name_letters--;

        set_dirty(DIRTY_VISUALIZER | DIRTY_FOOTER);

        return 0;
}

void playlist_save(void)
{
        Model *model = get_model();
        PlayList *playlist = get_playlist();

        if (num_playlist_name_letters < min_playlist_name_letters)
                return;

        export_current_playlist(model->settings.path, playlist, model->state.ui.playlist_name);

        model->state.ui.naming_playlist = false;

        set_dirty(DIRTY_VISUALIZER | DIRTY_FOOTER);
}

void set_save_playlist_mode(void)
{
        Model *model = get_model();

        model->state.ui.playlist_name[0] = '\0';
        model->state.ui.naming_playlist = true;
        num_playlist_name_letters = 0;
        num_playlist_name_bytes = 0;

        set_dirty(DIRTY_VISUALIZER | DIRTY_FOOTER);
}

char *get_playlist_name(void)
{
        Model *model = get_model();

        return model->state.ui.playlist_name;
}
