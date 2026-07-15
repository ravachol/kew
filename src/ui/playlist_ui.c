/**
 * @file playlist_ui.c
 * @brief Playlist display and interaction layer.
 *
 * Renders the list of tracks in the current playlist and handles
 * navigation, selection, and editing within the playlist view.
 */

#include "playlist_ui.h"

#include "ops/playback_state.h"
#include "ops/playback_system.h"

#include "common/appstate.h"

#include "ops/playlist_ops.h"
#include "sys/mpris.h"

#include <math.h>

Node *determine_start_node(Node *head, int *found_at, int list_size)
{
        if (found_at == NULL) {
                return head;
        }

        Node *node = head;
        Node *current = get_current_song();
        Node *found_node = NULL;
        int num_songs = 0;
        *found_at = -1;

        if (!current)
                return head;

        while (node != NULL && num_songs <= list_size) {
                if (current != NULL && current->id == node->id) {
                        *found_at = num_songs;
                        found_node = node;
                        break;
                }
                node = node->next;
                num_songs++;
        }

        return found_node ? found_node : head;
}

void ensure_chosen_song_within_limits(int *chosen_song, PlayList *list)
{
        if (list == NULL) {
                *chosen_song = 0;
        } else if (*chosen_song >= list->count) {
                *chosen_song = list->count - 1;
        }

        if (*chosen_song < 0) {
                *chosen_song = 0;
        }
}

int determine_playlist_start(int *previous_start_iter, int found_at, int max_list_size,
                             int *chosen_song, bool reset, bool end_of_list_reached, bool center)
{
        int start_iter = 0;

        start_iter = (found_at > -1 && (found_at > start_iter + max_list_size))
                         ? found_at
                         : start_iter;

        if (!center) {
                if (*previous_start_iter <= found_at &&
                    found_at < *previous_start_iter + max_list_size)
                        start_iter = *previous_start_iter;

                if (*chosen_song < start_iter) {

                        start_iter = *chosen_song;
                }
        }

        int half = max_list_size - floor(max_list_size / 2);

        if (start_iter < half)
                start_iter = 0;

        if (center) {
                start_iter = *previous_start_iter;

                if (*chosen_song >= start_iter) {

                        if (start_iter >= *chosen_song - half)
                                start_iter--;

                        if (start_iter < *chosen_song - half)
                                start_iter++;
                }

                if (*chosen_song < start_iter)
                        start_iter = *chosen_song - half;

        }


        if (*chosen_song > start_iter + max_list_size) {
                start_iter = *previous_start_iter;

                if (*chosen_song < start_iter || *chosen_song > start_iter + max_list_size)
                        start_iter = *chosen_song - half;
        }

        if (reset && !end_of_list_reached) {
                if (found_at > max_list_size)
                        start_iter = *previous_start_iter = *chosen_song = found_at;
                else
                        start_iter = *chosen_song = 0;
        }


        if (start_iter < 0)
                start_iter = 0;

        return start_iter;
}
