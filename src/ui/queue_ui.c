/**
 * @file queue_ui.c
 * @brief Handles high level enqueue
 *
 */

#include "queue_ui.h"

#include "common/appstate.h"

#include "data/directorytree.h"
#include "input.h"

#include "ops/library_ops.h"
#include "ops/playback_state.h"
#include "ops/playback_system.h"
#include "ops/playlist_ops.h"
#include "ops/track_manager.h"

#include "sound/sound_facade.h"
#include "sys/mpris.h"
#include "sys/sys_integration.h"

#include "utils/utils.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>

atomic_bool enqueueing = false;

void reset_list_after_dequeuing_playing_song(void)
{
        Model *model = get_model();
        AppState *state = &model->state;
        PlaybackState *ps = &model->playbackState;

        state->ui.startFromTop = true;

        if (ps->lastPlayedId < 0)
                return;

        Node *node = find_selected_entry_by_id(get_playlist(), ps->lastPlayedId);

        if (get_current_song() == NULL && node == NULL) {
                ps->loadedNextSong = false;
                sound_system_set_end_of_list_reached(sound_sys, true);
                start_playing(true);

                emit_metadata_changed("", "", "", "",
                                      "/org/mpris/MediaPlayer2/TrackList/NoTrack",
                                      NULL, 0);
                emit_playback_stopped_mpris();

                sound_system_uninit_device(sound_sys);

                set_dirty(DIRTY_ALL);

                switch_decoder();

                state->ui.songWasRemoved = true;

                start_playing(true);
                ps->waitingForNext = true;

                if (get_playlist()->count == 0)
                        set_song_to_start_from(NULL);
        }
}

bool starts_with_track_number(const char *name) {
    if (!isdigit((unsigned char)*name))
        return false;

    while (isdigit((unsigned char)*name))
        name++;

    // Accept common separators
    switch (*name) {
        case ' ':
        case '.':
        case '-':
        case '_':
            return true;
    }

    return false;
}

bool check_songs_for_track_number(FileSystemEntry* entry) {
    if (entry == NULL) return false;
    if (entry->is_directory) {
        if (entry->children == NULL) return false;
        return check_songs_for_track_number(entry->children);
    }
    else if (entry->next == NULL) {
        return false;
    }
    return (starts_with_track_number(entry->name) && starts_with_track_number(entry->next->name));
}

FileSystemEntry *enqueue_songs(FileSystemEntry *entry, FileSystemEntry **chosen_dir, bool dont_dequeue)
{
        Model *model = get_model();
        AppState *state = &model->state;
        FileSystemEntry *first_enqueued_entry = NULL;
        UIState *uis = &(state->ui);
        PlaybackState *ps = get_playback_state();
        bool has_enqueued = false;
        bool shuffle = false;

        if (entry != NULL) {
                if (entry->is_directory) {
                        if (!has_song_children(entry) || entry->parent == NULL ||
                            ((*chosen_dir) != NULL &&
                             strcmp(entry->full_path, (*chosen_dir)->full_path) == 0)) {
                                if (has_dequeued_children(entry)) {
                                        if (entry->parent == NULL) // Shuffle playlist if it's
                                                                   // the root
                                                shuffle = true;

                                        if (count_music_files_in_directory(entry) > MAX_SORT_SIZE ||
                                            check_songs_for_track_number(entry) // songs are already ordered if track number is in name
                                        ) {
                                            has_enqueued = enqueue_children(entry->children, &first_enqueued_entry);
                                        }
                                        else {
                                            has_enqueued = enqueue_children_sorted(entry->children, &first_enqueued_entry);
                                        }

                                        ps->nextSongNeedsRebuilding = true;
                                } else if (!dont_dequeue) {
                                        dequeue_children(entry);

                                        ps->nextSongNeedsRebuilding = true;
                                }
                        }

                        // FIXME move this stuff out of here and into update_view_state functions in update.c
                        if (state->currentView == LIBRARY_VIEW && state->ui.current_lib_entry && state->ui.current_lib_entry->is_directory)
                                *chosen_dir = state->ui.current_lib_entry;

                        if (state->currentView == SEARCH_VIEW && state->ui.current_search_entry && state->ui.current_search_entry->is_directory)
                                *chosen_dir = state->ui.current_search_entry;

                        if (state->currentView == LIBRARY_VIEW && entry->parent != NULL)
                                uis->allowChooseSongs = true;

                        if (state->currentView == SEARCH_VIEW && entry->parent != NULL)
                                uis->allowChooseSearchSongs = true;
                } else {
                        if (!entry->is_enqueued) {
                                set_next_song(NULL);
                                ps->nextSongNeedsRebuilding = true;
                                first_enqueued_entry = entry;

                                enqueue_song(entry);
                                set_childrens_queued_status_on_parents(entry->parent, true);

                                has_enqueued = true;
                        } else {
                                set_next_song(NULL);
                                ps->nextSongNeedsRebuilding = true;

                                if (!dont_dequeue) {
                                        dequeue_song(entry);
                                }
                                else {
                                        first_enqueued_entry = entry;
                                }
                                set_childrens_queued_status_on_parents(entry->parent, false);
                        }
                }
        }

        if (has_enqueued && first_enqueued_entry) {
                autostart_if_stopped(first_enqueued_entry->full_path);
        }

        if (shuffle) {
                PlayList *playlist = get_playlist();

                shuffle_playlist(playlist);
                set_song_to_start_from(NULL);
                first_enqueued_entry = NULL;
        } else if (ps->nextSongNeedsRebuilding) {
                reshuffle_playlist();
        }

        return first_enqueued_entry;
}

Node *enqueue_playlist(FileSystemEntry *entry, bool dont_dequeue)
{
        Model *model = get_model();
        AppState *state = &model->state;
        PlaybackState *ps = &model->playbackState;
        PlayList *playlist = model->playlist;
        Node *first_enqueued_node = NULL;

        if (should_start_playing()) {
                Node *last_song = find_selected_entry_by_id(playlist, ps->lastPlayedId);
                state->ui.startFromTop = false;

                if (last_song == NULL) {
                        if (playlist->tail != NULL)
                                ps->lastPlayedId = playlist->tail->id;
                        else {
                                ps->lastPlayedId = -1;
                                state->ui.startFromTop = true;
                        }
                }
        }

        pthread_mutex_lock(&(playlist->mutex));

        if (!entry->is_enqueued) {
                set_next_song(NULL);
                ps->nextSongNeedsRebuilding = true;

                enqueue_m3u(entry->full_path, get_library(), &first_enqueued_node, dont_dequeue);

                if (state->settings.shuffle_enabled)
                        reshuffle_playlist();

                entry->is_enqueued = 1;
        } else if (!dont_dequeue) {
                set_next_song(NULL);
                ps->nextSongNeedsRebuilding = true;

                dequeue_m3u(entry->full_path, get_library());
                entry->is_enqueued = 0;
        }
        else {
                set_next_song(NULL);
                ps->nextSongNeedsRebuilding = true;
                enqueue_m3u(entry->full_path, get_library(), &first_enqueued_node, dont_dequeue);
        }

        reset_list_after_dequeuing_playing_song();

        pthread_mutex_unlock(&(playlist->mutex));

        set_dirty(DIRTY_ALL);

        return first_enqueued_node;
}

FileSystemEntry *get_chosen_dir(void)
{
        Model *model = get_model();
        return  model->state.ui.chosen_dir;
}

void set_chosen_dir(FileSystemEntry *entry)
{
        Model *model = get_model();

        if (entry == NULL) {
                return;
        }

        if (entry->is_directory) {
                model->state.ui.current_lib_entry = model->state.ui.chosen_dir = entry;
        }
}

FileSystemEntry *enqueue(FileSystemEntry *entry, bool dont_dequeue)
{
        Model *model = get_model();
        AppState *state = &model->state;
        FileSystemEntry *first_enqueued_entry = NULL;
        PlaybackState *ps = &model->playbackState;

        if (should_start_playing()) {
                Node *last_song = find_selected_entry_by_id(get_playlist(), ps->lastPlayedId);
                state->ui.startFromTop = false;

                if (last_song == NULL) {
                        if (get_playlist()->tail != NULL)
                                ps->lastPlayedId = get_playlist()->tail->id;
                        else {
                                ps->lastPlayedId = -1;
                                state->ui.startFromTop = true;
                        }
                }
        }

        pthread_mutex_lock(&(get_playlist()->mutex));

        FileSystemEntry *chosen_dir = NULL;

        if (state->currentView == LIBRARY_VIEW)
                chosen_dir = model->state.ui.chosen_dir;

        if (state->currentView == SEARCH_VIEW)
                 chosen_dir = model->state.ui.chosen_search_dir;

        first_enqueued_entry = enqueue_songs(entry, &chosen_dir, dont_dequeue);

        if (state->currentView == LIBRARY_VIEW)
                model->state.ui.chosen_dir = chosen_dir;

        if (state->currentView == SEARCH_VIEW)
                model->state.ui.chosen_search_dir = chosen_dir;

        reset_list_after_dequeuing_playing_song();

        pthread_mutex_unlock(&(get_playlist()->mutex));

        return first_enqueued_entry;
}

Node *pick_random_node(Node *first)
{
        int count = 0;
        Node *n = first;

        while (n) {
                count++;
                n = n->next;
        }

        int idx = rand() % count;

        n = first;
        while (idx--)
                n = n->next;

        return n;
}

void view_enqueue(bool play_immediately)
{
        if (atomic_exchange(&enqueueing, true)) {
                return; // already true → bail
        }

        AppState *state = get_app_state();
        PlayList *playlist = get_playlist();
        PlaybackState *ps = get_playback_state();
        PlayList *unshuffled_playlist = get_unshuffled_playlist();

        FileSystemEntry *entry = NULL;
        Node *current_song = get_current_song();
        Node *first_enqueued_node = NULL;
        bool canGoNext = (current_song != NULL && current_song->next != NULL);

        if (state->currentView == TRACK_VIEW) {
                if (current_song != NULL) {
                        clear_and_play(current_song);
                }
        }

        if (state->currentView == PLAYLIST_VIEW || is_digits_pressed() > 0) {
                if (is_digits_pressed() == 0) {
                        playlist_play(playlist);
                } else {
                        state->ui.resetPlaylistDisplay = true;
                        int song_number = get_number_from_string(get_digits_pressed());
                        reset_digits_pressed();

                        ps->nextSongNeedsRebuilding = false;

                        if (song_number <= playlist->count)
                                skip_to_numbered_song(song_number);
                }

                set_dirty(DIRTY_PLAYLIST);
        }

        if (state->currentView == LIBRARY_VIEW || state->currentView == SEARCH_VIEW) {

                if (state->currentView == LIBRARY_VIEW)
                        entry = state->ui.current_lib_entry;
                else {
                        entry = state->ui.current_search_entry;
                }

                if (entry == NULL) {
                        atomic_store(&enqueueing, false);
                        return;
                }

                // Enqueue / dequeue playlist file (toggle, mirroring directory behaviour)
                if (is_m3u_file(entry)) {
                        first_enqueued_node = enqueue_playlist(entry, play_immediately);
                } else {
                        FileSystemEntry *first_enqueued_entry = enqueue(entry, play_immediately); // Enqueue song
                        if (first_enqueued_entry)
                                first_enqueued_node = find_path_in_playlist(first_enqueued_entry->full_path, playlist);
                }

                set_dirty(DIRTY_LIBRARY | DIRTY_SEARCH);
        }

        if (first_enqueued_node && state->settings.shuffle_enabled) {
                Node *unshuffled_node = find_path_in_playlist(first_enqueued_node->song.file_path, unshuffled_playlist);
                if (unshuffled_node && unshuffled_node->next) {
                        Node *rand = pick_random_node(unshuffled_node);
                        first_enqueued_node = find_path_in_playlist(rand->song.file_path, playlist);
                }
        }

        if (first_enqueued_node && (play_immediately || is_stopped()) && playlist->count != 0) {
                clear_and_play(first_enqueued_node);
        }

        // Handle MPRIS can_go_next
        current_song = get_current_song();
        bool couldGoNext = (current_song != NULL && current_song->next != NULL);
        if (canGoNext != couldGoNext) {
                emit_boolean_property_changed("can_go_next", couldGoNext);
        }

        atomic_store(&enqueueing, false);
}
