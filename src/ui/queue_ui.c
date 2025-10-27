/**
 * @file queue_ui.c
 * @brief Handles high level enqueue
 *
 */

#include "queue_ui.h"

#include "common/appstate.h"
#include "common/common.h"

#include "input.h"
#include "player_ui.h"
#include "search_ui.h"

#include "ops/library_ops.h"
#include "ops/playback_clock.h"
#include "ops/playback_state.h"
#include "ops/playback_system.h"
#include "ops/playlist_ops.h"
#include "ops/track_manager.h"

#include "sys/mpris.h"
#include "sys/sys_integration.h"

#include "utils/utils.h"

#include <locale.h>

void determine_song_and_notify(void)
{
        AppState *state = get_app_state();
        SongData *current_song_data = NULL;
        bool isDeleted = determine_current_song_data(&current_song_data);
        Node *current = get_current_song();

        if (current_song_data && current)
                current->song.duration = current_song_data->duration;

        if (state->uiState.lastNotifiedId != current->id) {
                if (!isDeleted)
                        notify_song_switch(current_song_data);
        }
}

void update_next_song_if_needed(void)
{
        load_next_song();
        determine_song_and_notify();
}

void reset_list_after_dequeuing_playing_song(void)
{
        AppState *state = get_app_state();
        PlaybackState *ps = get_playback_state();

        state->uiState.startFromTop = true;

        if (ps->lastPlayedId < 0)
                return;

        Node *node = find_selected_entry_by_id(get_playlist(), ps->lastPlayedId);

        if (get_current_song() == NULL && node == NULL) {
                ps->loadedNextSong = false;
                audio_data.end_of_list_reached = true;
                audio_data.restart = true;

                emit_metadata_changed("", "", "", "",
                                      "/org/mpris/MediaPlayer2/TrackList/NoTrack",
                                      NULL, 0);
                emit_playback_stopped_mpris();

                playback_cleanup();

                trigger_refresh();

                playback_switch_decoder();

                unload_song_a();
                unload_song_b();

                state->uiState.songWasRemoved = true;

                UserData *user_data = audio_data.pUserData;

                user_data->current_song_data = NULL;

                audio_data.currentFileIndex = 0;
                audio_data.restart = true;
                ps->waitingForNext = true;

                PlaybackState *ps = get_playback_state();

                ps->loadingdata.loadA = true;
                ps->usingSongDataA = false;

                ma_data_source_uninit(&audio_data);

                audio_data.switchFiles = false;

                if (get_playlist()->count == 0)
                        set_song_to_start_from(NULL);
        }
}

FileSystemEntry *enqueue_songs(FileSystemEntry *entry, FileSystemEntry **chosen_dir)
{
        AppState *state = get_app_state();
        FileSystemEntry *first_enqueued_entry = NULL;
        UIState *uis = &(state->uiState);
        PlaybackState *ps = get_playback_state();
        bool has_enqueued = false;
        bool shuffle = false;

        if (entry != NULL) {
                if (entry->is_directory) {
                        if (!has_song_children(entry) || entry->parent == NULL ||
                            ((*chosen_dir) != NULL &&
                             strcmp(entry->full_path, (*chosen_dir)->full_path) == 0)) {
                                if (has_dequeued_children(entry)) {
                                        if (entry->parent ==
                                            NULL) // Shuffle playlist if it's
                                                  // the root
                                                shuffle = true;

                                        entry->is_enqueued = enqueue_children(entry->children, &first_enqueued_entry);

                                        has_enqueued = entry->is_enqueued;

                                        ps->nextSongNeedsRebuilding = true;
                                } else {
                                        dequeue_children(entry);

                                        entry->is_enqueued = 0;

                                        ps->nextSongNeedsRebuilding = true;
                                }
                        }
                        if ((*chosen_dir) != NULL && entry->parent != NULL &&
                            is_contained_within(entry, (*chosen_dir)) &&
                            uis->allowChooseSongs == true) {
                                // If the chosen directory is the same as the
                                // entry's parent and it is open
                                uis->openedSubDir = true;

                                FileSystemEntry *tmpc = (*chosen_dir)->children;

                                uis->numSongsAboveSubDir = 0;

                                while (tmpc != NULL) {
                                        if (strcmp(entry->full_path,
                                                   tmpc->full_path) == 0 ||
                                            is_contained_within(entry, tmpc))
                                                break;
                                        tmpc = tmpc->next;
                                        uis->numSongsAboveSubDir++;
                                }
                        }

                        AppState *state = get_app_state();

                        if (state->uiState.currentLibEntry && state->uiState.currentLibEntry->is_directory)
                                *chosen_dir = state->uiState.currentLibEntry;

                        if (uis->allowChooseSongs == true) {
                                uis->collapseView = true;
                                trigger_refresh();
                        }

                        if (entry->parent != NULL)
                                uis->allowChooseSongs = true;
                } else {
                        if (!entry->is_enqueued) {
                                set_next_song(NULL);
                                ps->nextSongNeedsRebuilding = true;
                                first_enqueued_entry = entry;

                                enqueue_song(entry);

                                has_enqueued = true;
                        } else {
                                set_next_song(NULL);
                                ps->nextSongNeedsRebuilding = true;

                                dequeue_song(entry);
                        }
                }
                trigger_refresh();
        }

        if (has_enqueued) {
                autostart_if_stopped(first_enqueued_entry);
        }

        if (shuffle) {
                PlayList *playlist = get_playlist();

                shuffle_playlist(playlist);
                set_song_to_start_from(NULL);
        }

        if (ps->nextSongNeedsRebuilding) {
                reshuffle_playlist();
        }

        return first_enqueued_entry;
}

FileSystemEntry *enqueue(FileSystemEntry *entry)
{
        AppState *state = get_app_state();
        FileSystemEntry *first_enqueued_entry = NULL;
        PlaybackState *ps = get_playback_state();

        if (audio_data.restart) {
                Node *last_song = find_selected_entry_by_id(get_playlist(), ps->lastPlayedId);
                state->uiState.startFromTop = false;

                if (last_song == NULL) {
                        if (get_playlist()->tail != NULL)
                                ps->lastPlayedId = get_playlist()->tail->id;
                        else {
                                ps->lastPlayedId = -1;
                                state->uiState.startFromTop = true;
                        }
                }
        }

        pthread_mutex_lock(&(get_playlist()->mutex));

        FileSystemEntry *chosen_dir = get_chosen_dir();
        first_enqueued_entry = enqueue_songs(entry, &chosen_dir);
        set_chosen_dir(chosen_dir);
        reset_list_after_dequeuing_playing_song();

        pthread_mutex_unlock(&(get_playlist()->mutex));

        return first_enqueued_entry;
}

void view_enqueue(bool play_immediately)
{
        AppState *state = get_app_state();
        PlayList *playlist = get_playlist();
        PlayList *unshuffled_playlist = get_unshuffled_playlist();
        PlaybackState *ps = get_playback_state();
        FileSystemEntry *library = get_library();
        FileSystemEntry *first_enqueued_entry = NULL;
        Node *current_song = get_current_song();
        bool canGoNext = (current_song != NULL && current_song->next != NULL);

        if (state->currentView == TRACK_VIEW) {
                if (current_song != NULL) {
                        clear_and_play(current_song);
                }
        }

        if (state->currentView == PLAYLIST_VIEW) {
                if (is_digits_pressed() == 0) {
                        playlist_play(playlist);
                } else {
                        state->uiState.resetPlaylistDisplay = true;
                        int song_number = get_number_from_string(get_digits_pressed());
                        reset_digits_pressed();

                        ps->nextSongNeedsRebuilding = false;

                        skip_to_numbered_song(song_number);
                }
        }

        if (state->currentView == LIBRARY_VIEW) {
                FileSystemEntry *entry = state->uiState.currentLibEntry;

                if (entry == NULL)
                        return;

                // Enqueue playlist
                if (path_ends_with(entry->full_path, "m3u") ||
                    path_ends_with(entry->full_path, "m3u8")) {
                        FileSystemEntry *first_enqueued_entry = NULL;

                        Node *prev_tail = playlist->tail;

                        if (playlist != NULL) {
                                read_m3_u_file(entry->full_path, playlist, library);

                                if (prev_tail != NULL && prev_tail->next != NULL) {
                                        first_enqueued_entry = find_corresponding_entry(
                                            library, prev_tail->next->song.file_path);
                                } else if (playlist->head != NULL) {
                                        first_enqueued_entry = find_corresponding_entry(
                                            library, playlist->head->song.file_path);
                                }

                                autostart_if_stopped(first_enqueued_entry);
                                mark_list_as_enqueued(library, playlist);
                                deep_copy_play_list_onto_list(playlist, &unshuffled_playlist);
                        }
                } else
                        first_enqueued_entry = enqueue(entry); // Enqueue song
        }

        if (state->currentView == SEARCH_VIEW) {
                set_chosen_dir(get_current_search_entry());
                first_enqueued_entry = enqueue(get_current_search_entry());
        }

        if (first_enqueued_entry && play_immediately && playlist->count != 0) {
                Node *song = find_path_in_playlist(first_enqueued_entry->full_path, playlist);
                clear_and_play(song);
        }

        // Handle MPRIS can_go_next
        bool couldGoNext = (current_song != NULL && current_song->next != NULL);
        if (canGoNext != couldGoNext) {
                emit_boolean_property_changed("can_go_next", couldGoNext);
        }
}
