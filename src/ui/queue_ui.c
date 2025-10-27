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
        SongData *currentSongData = NULL;
        bool isDeleted = determine_current_song_data(&currentSongData);
        Node *current = get_current_song();

        if (currentSongData && current)
                current->song.duration = currentSongData->duration;

        if (state->uiState.lastNotifiedId != current->id)
        {
                if (!isDeleted)
                        notify_song_switch(currentSongData);
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

        if (get_current_song() == NULL && node == NULL)
        {
                ps->loadedNextSong = false;
                audio_data.endOfListReached = true;
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

                UserData *userData = audio_data.pUserData;

                userData->currentSongData = NULL;

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
        FileSystemEntry *firstEnqueuedEntry = NULL;
        UIState *uis = &(state->uiState);
        PlaybackState *ps = get_playback_state();
        bool hasEnqueued = false;
        bool shuffle = false;

        if (entry != NULL)
        {
                if (entry->is_directory)
                {
                        if (!has_song_children(entry) || entry->parent == NULL ||
                            ((*chosen_dir) != NULL &&
                             strcmp(entry->fullPath, (*chosen_dir)->fullPath) == 0))
                        {
                                if (has_dequeued_children(entry))
                                {
                                        if (entry->parent ==
                                            NULL) // Shuffle playlist if it's
                                                  // the root
                                                shuffle = true;

                                        entry->isEnqueued = enqueue_children(entry->children, &firstEnqueuedEntry);

                                        hasEnqueued = entry->isEnqueued;

                                        ps->nextSongNeedsRebuilding = true;
                                }
                                else
                                {
                                        dequeue_children(entry);

                                        entry->isEnqueued = 0;

                                        ps->nextSongNeedsRebuilding = true;
                                }
                        }
                        if ((*chosen_dir) != NULL && entry->parent != NULL &&
                            is_contained_within(entry, (*chosen_dir)) &&
                            uis->allowChooseSongs == true)
                        {
                                // If the chosen directory is the same as the
                                // entry's parent and it is open
                                uis->openedSubDir = true;

                                FileSystemEntry *tmpc = (*chosen_dir)->children;

                                uis->numSongsAboveSubDir = 0;

                                while (tmpc != NULL)
                                {
                                        if (strcmp(entry->fullPath,
                                                   tmpc->fullPath) == 0 ||
                                            is_contained_within(entry, tmpc))
                                                break;
                                        tmpc = tmpc->next;
                                        uis->numSongsAboveSubDir++;
                                }
                        }

                        AppState *state = get_app_state();

                        if (state->uiState.currentLibEntry && state->uiState.currentLibEntry->is_directory)
                                *chosen_dir = state->uiState.currentLibEntry;

                        if (uis->allowChooseSongs == true)
                        {
                                uis->collapseView = true;
                                trigger_refresh();
                        }

                        if (entry->parent != NULL)
                                uis->allowChooseSongs = true;
                }
                else
                {
                        if (!entry->isEnqueued)
                        {
                                set_next_song(NULL);
                                ps->nextSongNeedsRebuilding = true;
                                firstEnqueuedEntry = entry;

                                enqueue_song(entry);

                                hasEnqueued = true;
                        }
                        else
                        {
                                set_next_song(NULL);
                                ps->nextSongNeedsRebuilding = true;

                                dequeue_song(entry);
                        }
                }
                trigger_refresh();
        }

        if (hasEnqueued)
        {
                autostart_if_stopped(firstEnqueuedEntry);
        }

        if (shuffle)
        {
                PlayList *playlist = get_playlist();

                shuffle_playlist(playlist);
                set_song_to_start_from(NULL);
        }

        if (ps->nextSongNeedsRebuilding)
        {
                reshuffle_playlist();
        }

        return firstEnqueuedEntry;
}

FileSystemEntry *enqueue(FileSystemEntry *entry)
{
        AppState *state = get_app_state();
        FileSystemEntry *firstEnqueuedEntry = NULL;
        PlaybackState *ps = get_playback_state();

        if (audio_data.restart)
        {
                Node *lastSong = find_selected_entry_by_id(get_playlist(), ps->lastPlayedId);
                state->uiState.startFromTop = false;

                if (lastSong == NULL)
                {
                        if (get_playlist()->tail != NULL)
                                ps->lastPlayedId = get_playlist()->tail->id;
                        else
                        {
                                ps->lastPlayedId = -1;
                                state->uiState.startFromTop = true;
                        }
                }
        }

        pthread_mutex_lock(&(get_playlist()->mutex));

        FileSystemEntry *chosen_dir = get_chosen_dir();
        firstEnqueuedEntry = enqueue_songs(entry, &chosen_dir);
        set_chosen_dir(chosen_dir);
        reset_list_after_dequeuing_playing_song();

        pthread_mutex_unlock(&(get_playlist()->mutex));

        return firstEnqueuedEntry;
}

void view_enqueue(bool playImmediately)
{
        AppState *state = get_app_state();
        PlayList *playlist = get_playlist();
        PlayList *unshuffled_playlist = get_unshuffled_playlist();
        PlaybackState *ps = get_playback_state();
        FileSystemEntry *library = get_library();
        FileSystemEntry *firstEnqueuedEntry = NULL;
        Node *current_song = get_current_song();
        bool canGoNext = (current_song != NULL && current_song->next != NULL);

        if (state->currentView == TRACK_VIEW)
        {
                if (current_song != NULL)
                {
                        clear_and_play(current_song);
                }
        }

        if (state->currentView == PLAYLIST_VIEW)
        {
                if (is_digits_pressed() == 0)
                {
                        playlist_play(playlist);
                }
                else
                {
                        state->uiState.resetPlaylistDisplay = true;
                        int songNumber = get_number_from_string(get_digits_pressed());
                        reset_digits_pressed();

                        ps->nextSongNeedsRebuilding = false;

                        skip_to_numbered_song(songNumber);
                }
        }

        if (state->currentView == LIBRARY_VIEW)
        {
                FileSystemEntry *entry = state->uiState.currentLibEntry;

                if (entry == NULL)
                        return;

                // Enqueue playlist
                if (path_ends_with(entry->fullPath, "m3u") ||
                    path_ends_with(entry->fullPath, "m3u8"))
                {
                        FileSystemEntry *firstEnqueuedEntry = NULL;

                        Node *prevTail = playlist->tail;

                        if (playlist != NULL)
                        {
                                read_m3_u_file(entry->fullPath, playlist, library);

                                if (prevTail != NULL && prevTail->next != NULL)
                                {
                                        firstEnqueuedEntry = find_corresponding_entry(
                                            library, prevTail->next->song.filePath);
                                }
                                else if (playlist->head != NULL)
                                {
                                        firstEnqueuedEntry = find_corresponding_entry(
                                            library, playlist->head->song.filePath);
                                }

                                autostart_if_stopped(firstEnqueuedEntry);
                                mark_list_as_enqueued(library, playlist);
                                deep_copy_play_list_onto_list(playlist, &unshuffled_playlist);
                        }
                }
                else
                        firstEnqueuedEntry = enqueue(entry); // Enqueue song
        }

        if (state->currentView == SEARCH_VIEW)
        {
                set_chosen_dir(get_current_search_entry());
                firstEnqueuedEntry = enqueue(get_current_search_entry());
        }

        if (firstEnqueuedEntry && playImmediately && playlist->count != 0)
        {
                Node *song = find_path_in_playlist(firstEnqueuedEntry->fullPath, playlist);
                clear_and_play(song);
        }

        // Handle MPRIS can_go_next
        bool couldGoNext = (current_song != NULL && current_song->next != NULL);
        if (canGoNext != couldGoNext)
        {
                emit_boolean_property_changed("can_go_next", couldGoNext);
        }
}

