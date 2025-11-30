/**
 * @file playlist_ui.c
 * @brief Playlist display and interaction layer.
 *
 * Renders the list of tracks in the current playlist and handles
 * navigation, selection, and editing within the playlist view.
 */

#include "playlist_ui.h"

#include "common/common.h"
#include "ops/playback_state.h"
#include "ops/playback_system.h"

#include "common/appstate.h"
#include "common_ui.h"

#include "data/song_loader.h"

#include "ops/playlist_ops.h"
#include "sys/mpris.h"
#include "utils/term.h"
#include "utils/utils.h"

#include <math.h>
#include <string.h>

static const int MAX_TERM_WIDTH = 1000;

static int start_iter = 0;
static int previous_chosen_song = 0;
static bool is_same_name_as_last_time = false;

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

void prepare_playlist_string(Node *node, char *buffer, int buffer_size)
{
        if (node == NULL || buffer == NULL || node->song.file_path == NULL ||
            buffer_size <= 0) {
                if (buffer && buffer_size > 0)
                        buffer[0] = '\0';
                return;
        }

        if (strnlen(node->song.file_path, PATH_MAX) >= PATH_MAX) {
                buffer[0] = '\0';
                return;
        }

        char file_path[PATH_MAX];
        c_strcpy(file_path, node->song.file_path, sizeof(file_path));

        char *last_slash = strrchr(file_path, '/');
        size_t len = strnlen(file_path, sizeof(file_path));

        if (last_slash != NULL && last_slash < file_path + len) {
                c_strcpy(buffer, last_slash + 1, buffer_size);
                buffer[buffer_size - 1] = '\0';
        } else {
                // If no slash found or invalid pointer arithmetic, just copy
                // whole path safely or clear
                c_strcpy(buffer, file_path, buffer_size);
                buffer[buffer_size - 1] = '\0';
        }
}

int display_playlist_items(int row, int col, Node *start_node, int start_iter, int max_list_size,
                           int term_width, int chosen_song,
                           int *chosen_node_id, UISettings *ui)
{
        int num_printed_rows = 0;
        Node *node = start_node;
        int indent = col -1;

        if (term_width < 0 || term_width > MAX_TERM_WIDTH || indent < 0 ||
            indent >= term_width)
                return 0;

        int max_name_width = term_width - indent - (indent / 4) - 10;
        if (max_name_width <= 0)
                return 0;

        unsigned char default_color = ui->default_color;

        PixelData row_color = {default_color, default_color, default_color};
        PixelData row_color2 = {default_color, default_color, default_color};

        char *buffer = malloc(NAME_MAX + 1);
        if (!buffer)
                return 0;

        char *filename = malloc(NAME_MAX + 1);
        if (!filename) {
                free(buffer);
                return 0;
        }

        for (int i = start_iter; node != NULL && i < start_iter + max_list_size;
             i++) {
                PixelData rgbRowNum = {default_color, default_color, default_color};
                PixelData rgbTitle = {default_color, default_color, default_color};

                if (ui->colorMode == COLOR_MODE_THEME &&
                    ui->theme.playlist_rownum.type == COLOR_TYPE_RGB) {
                        rgbRowNum = ui->theme.playlist_rownum.rgb;
                        rgbTitle = ui->theme.playlist_title.rgb;
                } else {
                        rgbRowNum = ui->color;
                }

                if (!(rgbRowNum.r == default_color &&
                      rgbRowNum.g == default_color &&
                      rgbRowNum.b == default_color))
                        row_color = get_gradient_color(rgbRowNum, i - start_iter,
                                                       max_list_size,
                                                       max_list_size / 2, 0.7f);

                if (!(rgbTitle.r == default_color &&
                      rgbTitle.g == default_color &&
                      rgbTitle.b == default_color))
                        row_color2 = get_gradient_color(rgbTitle, i - start_iter,
                                                        max_list_size,
                                                        max_list_size / 2, 0.7f);

                prepare_playlist_string(node, buffer, NAME_MAX);

                if (buffer[0] != '\0') {
                        if (ui->colorMode == COLOR_MODE_ALBUM || (ui->colorMode == COLOR_MODE_THEME &&
                            ui->theme.playlist_rownum.type == COLOR_TYPE_RGB))
                                apply_color(COLOR_MODE_ALBUM, ui->theme.playlist_rownum,
                                            row_color);
                        else
                                apply_color(ui->colorMode, ui->theme.playlist_rownum,
                                            row_color);

                        printf("\033[%d;%dH", row + num_printed_rows, col);
                        clear_rest_of_line();

                        printf("   %d. ", i + 1);

                        if (ui->colorMode == COLOR_MODE_ALBUM || (ui->colorMode == COLOR_MODE_THEME &&
                            ui->theme.playlist_rownum.type == COLOR_TYPE_RGB))
                                apply_color(COLOR_MODE_ALBUM, ui->theme.playlist_title,
                                            row_color2);
                        else
                                apply_color(ui->colorMode, ui->theme.playlist_title,
                                            ui->defaultColorRGB);

                        is_same_name_as_last_time =
                            (previous_chosen_song == chosen_song);

                        if (!is_same_name_as_last_time) {
                                reset_name_scroll();
                        }

                        filename[0] = '\0';

                        if (i == chosen_song) {
                                previous_chosen_song = chosen_song;

                                *chosen_node_id = node->id;

                                process_name_scroll(buffer, filename,
                                                    max_name_width,
                                                    is_same_name_as_last_time);

                                inverse_text();
                        } else {
                                process_name(buffer, filename, max_name_width,
                                             true, true);
                        }

                        Node *current = get_current_song();

                        if (current != NULL && current->id == node->id)
                                apply_color(ui->colorMode,
                                            ui->theme.playlist_playing,
                                            row_color);

                        if (i + 1 < 10)
                                printf(" ");

                        if (current != NULL && current->id == node->id &&
                            i == chosen_song) {
                                inverse_text();
                        }

                        if (current != NULL && current->id == node->id &&
                            i != chosen_song) {
                                printf("\e[4m");
                        }

                        printf("%s", filename);

                        num_printed_rows++;
                }

                node = node->next;

                reset_color();
        }

        free(buffer);
        free(filename);

        return num_printed_rows;
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

int determine_playlist_start(int previous_start_iter, int found_at, int max_list_size,
                             int *chosen_song, bool reset, bool end_of_list_reached)
{
        int start_iter = 0;

        start_iter = (found_at > -1 && (found_at > start_iter + max_list_size))
                         ? found_at
                         : start_iter;

        if (previous_start_iter <= found_at &&
            found_at < previous_start_iter + max_list_size)
                start_iter = previous_start_iter;

        if (*chosen_song < start_iter) {
                start_iter = *chosen_song;
        }

        if (*chosen_song > start_iter + max_list_size - floor(max_list_size / 2)) {
                start_iter = *chosen_song - max_list_size + floor(max_list_size / 2);
        }

        if (reset && !end_of_list_reached) {
                if (found_at > max_list_size)
                        start_iter = previous_start_iter = *chosen_song = found_at;
                else
                        start_iter = *chosen_song = 0;
        }

        return start_iter;
}

void move_start_node_into_position(int found_at, Node **start_node)
{
        // Go up to adjust the start_node
        for (int i = found_at; i > start_iter; i--) {
                if (i > 0 && (*start_node)->prev != NULL)
                        *start_node = (*start_node)->prev;
        }

        // Go down to adjust the start_node
        for (int i = (found_at == -1) ? 0 : found_at; i < start_iter; i++) {
                if ((*start_node)->next != NULL)
                        *start_node = (*start_node)->next;
        }
}

int display_playlist(int row, int col, PlayList *list, int max_list_size,
                     int *chosen_song, int *chosen_node_id, bool reset)
{
        AppState *state = get_app_state();
        int term_width, termHeight;
        get_term_size(&term_width, &termHeight);

        UISettings *ui = &(state->uiSettings);

        int found_at = -1;

        Node *start_node = NULL;
        if (list != NULL)
                start_node = determine_start_node(list->head, &found_at, list->count);

        ensure_chosen_song_within_limits(chosen_song, list);

        start_iter = determine_playlist_start(start_iter, found_at, max_list_size, chosen_song,
                                              reset, audio_data.end_of_list_reached);

        move_start_node_into_position(found_at, &start_node);

        int printed_rows = display_playlist_items(row, col, start_node, start_iter, max_list_size, term_width,
                                                  *chosen_song, chosen_node_id, ui);

        while (printed_rows <= max_list_size) {
                printf("\033[%d;%dH", row + printed_rows, col);
                clear_rest_of_line();
                printed_rows++;
        }

        return printed_rows;
}

void set_end_of_list_reached(void)
{
        AppState *state = get_app_state();
        PlaybackState *ps = get_playback_state();

        ps->loadedNextSong = false;
        ps->waitingForNext = true;
        audio_data.end_of_list_reached = true;
        audio_data.currentFileIndex = 0;
        audio_data.restart = true;
        ps->usingSongDataA = false;
        ps->loadingdata.loadA = true;

        clear_current_song();

        playback_cleanup();

        trigger_refresh();

        if (is_repeat_list_enabled())
                repeat_list();
        else {
                emit_playback_stopped_mpris();
                emit_metadata_changed("", "", "", "",
                                      "/org/mpris/MediaPlayer2/TrackList/NoTrack",
                                      NULL, 0);
                state->currentView = LIBRARY_VIEW;
                clear_screen();
        }
}
