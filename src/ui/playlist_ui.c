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

Node *determine_start_node(Node *head, int *foundAt, int listSize)
{
        if (foundAt == NULL)
        {
                return head;
        }

        Node *node = head;
        Node *current = get_current_song();
        Node *foundNode = NULL;
        int numSongs = 0;
        *foundAt = -1;

        while (node != NULL && numSongs <= listSize)
        {
                if (current != NULL && current->id == node->id)
                {
                        *foundAt = numSongs;
                        foundNode = node;
                        break;
                }
                node = node->next;
                numSongs++;
        }

        return foundNode ? foundNode : head;
}

void prepare_playlist_string(Node *node, char *buffer, int bufferSize)
{
        if (node == NULL || buffer == NULL || node->song.filePath == NULL ||
            bufferSize <= 0)
        {
                if (buffer && bufferSize > 0)
                        buffer[0] = '\0';
                return;
        }

        if (strnlen(node->song.filePath, MAXPATHLEN) >= MAXPATHLEN)
        {
                buffer[0] = '\0';
                return;
        }

        char filePath[MAXPATHLEN];
        c_strcpy(filePath, node->song.filePath, sizeof(filePath));

        char *lastSlash = strrchr(filePath, '/');
        size_t len = strnlen(filePath, sizeof(filePath));

        if (lastSlash != NULL && lastSlash < filePath + len)
        {
                c_strcpy(buffer, lastSlash + 1, bufferSize);
                buffer[bufferSize - 1] = '\0';
        }
        else
        {
                // If no slash found or invalid pointer arithmetic, just copy
                // whole path safely or clear
                c_strcpy(buffer, filePath, bufferSize);
                buffer[bufferSize - 1] = '\0';
        }
}

int display_playlist_items(Node *startNode, int start_iter, int max_list_size,
                         int termWidth, int indent, int chosenSong,
                         int *chosenNodeId, UISettings *ui)
{
        int numPrintedRows = 0;
        Node *node = startNode;

        if (termWidth < 0 || termWidth > MAX_TERM_WIDTH || indent < 0 ||
            indent >= termWidth)
                return 0;

        int maxNameWidth = termWidth - indent - 12;
        if (maxNameWidth <= 0)
                return 0;

        unsigned char defaultColor = ui->defaultColor;

        PixelData rowColor = {defaultColor, defaultColor, defaultColor};
        PixelData rowColor2 = {defaultColor, defaultColor, defaultColor};

        char *buffer = malloc(NAME_MAX + 1);
        if (!buffer)
                return 0;

        char *filename = malloc(NAME_MAX + 1);
        if (!filename)
        {
                free(buffer);
                return 0;
        }

        for (int i = start_iter; node != NULL && i < start_iter + max_list_size;
             i++)
        {
                PixelData rgbRowNum = {defaultColor,defaultColor,defaultColor};
                PixelData rgbTitle = {defaultColor,defaultColor,defaultColor};

                if (ui->colorMode == COLOR_MODE_THEME &&
                        ui->theme.playlist_rownum.type == COLOR_TYPE_RGB)
                {
                        rgbRowNum = ui->theme.playlist_rownum.rgb;
                        rgbTitle = ui->theme.playlist_title.rgb;
                }
                else
                {
                        rgbRowNum = ui->color;
                }

                if (!(rgbRowNum.r == defaultColor &&
                      rgbRowNum.g == defaultColor &&
                      rgbRowNum.b == defaultColor))
                        rowColor = get_gradient_color(rgbRowNum, i - start_iter,
                                                    max_list_size,
                                                    max_list_size / 2, 0.7f);

                if (!(rgbTitle.r == defaultColor &&
                      rgbTitle.g == defaultColor &&
                      rgbTitle.b == defaultColor))
                        rowColor2 = get_gradient_color(rgbTitle, i - start_iter,
                                                    max_list_size,
                                                    max_list_size / 2, 0.7f);

                prepare_playlist_string(node, buffer, NAME_MAX);

                if (buffer[0] != '\0')
                {
                        if (ui->colorMode == COLOR_MODE_ALBUM || ui->colorMode == COLOR_MODE_THEME)
                                apply_color(COLOR_MODE_ALBUM, ui->theme.playlist_rownum,
                                        rowColor);
                        else
                                apply_color(ui->colorMode, ui->theme.playlist_rownum,
                                        rowColor);

                        clear_line();
                        print_blank_spaces(indent);
                        printf("   %d. ", i + 1);

                        if (ui->colorMode == COLOR_MODE_ALBUM || ui->colorMode == COLOR_MODE_THEME)
                                apply_color(COLOR_MODE_ALBUM, ui->theme.playlist_title,
                                        rowColor2);
                        else
                                apply_color(ui->colorMode, ui->theme.playlist_title,
                                        ui->defaultColorRGB);

                        is_same_name_as_last_time =
                            (previous_chosen_song == chosenSong);

                        if (!is_same_name_as_last_time)
                        {
                                reset_name_scroll();
                        }

                        filename[0] = '\0';

                        if (i == chosenSong)
                        {
                                previous_chosen_song = chosenSong;

                                *chosenNodeId = node->id;

                                process_name_scroll(buffer, filename,
                                                  maxNameWidth,
                                                  is_same_name_as_last_time);

                                inverse_text();
                        }
                        else
                        {
                                process_name(buffer, filename, maxNameWidth,
                                            true, true);
                        }

                        Node *current = get_current_song();

                        if (current != NULL && current->id == node->id)
                                apply_color(ui->colorMode,
                                           ui->theme.playlist_playing,
                                           rowColor);

                        if (i + 1 < 10)
                                printf(" ");

                        if (current != NULL && current->id == node->id &&
                            i == chosenSong)
                        {
                                inverse_text();
                        }

                        if (current != NULL && current->id == node->id &&
                            i != chosenSong)
                        {
                                printf("\e[4m");
                        }

                        printf("%s\n", filename);

                        numPrintedRows++;
                }

                node = node->next;

                reset_color();
        }

        free(buffer);
        free(filename);

        return numPrintedRows;
}

void ensure_chosen_song_within_limits(int *chosenSong, PlayList *list)
{
        if (list == NULL)
        {
                *chosenSong = 0;
        }
        else if (*chosenSong >= list->count)
        {
                *chosenSong = list->count - 1;
        }

        if (*chosenSong < 0)
        {
                *chosenSong = 0;
        }
}

int determine_playlist_start(int previousStartIter, int foundAt, int max_list_size,
                           int *chosenSong, bool reset, bool endOfListReached)
{
        int start_iter = 0;

        start_iter = (foundAt > -1 && (foundAt > start_iter + max_list_size))
                        ? foundAt
                        : start_iter;

        if (previousStartIter <= foundAt &&
            foundAt < previousStartIter + max_list_size)
                start_iter = previousStartIter;

        if (*chosenSong < start_iter)
        {
                start_iter = *chosenSong;
        }

        if (*chosenSong > start_iter + max_list_size - floor(max_list_size / 2))
        {
                start_iter = *chosenSong - max_list_size + floor(max_list_size / 2);
        }

        if (reset && !endOfListReached)
        {
                if (foundAt > max_list_size)
                        start_iter = previousStartIter = *chosenSong = foundAt;
                else
                        start_iter = *chosenSong = 0;
        }

        return start_iter;
}

void move_start_node_into_position(int foundAt, Node **startNode)
{
        // Go up to adjust the startNode
        for (int i = foundAt; i > start_iter; i--)
        {
                if (i > 0 && (*startNode)->prev != NULL)
                        *startNode = (*startNode)->prev;
        }

        // Go down to adjust the startNode
        for (int i = (foundAt == -1) ? 0 : foundAt; i < start_iter; i++)
        {
                if ((*startNode)->next != NULL)
                        *startNode = (*startNode)->next;
        }
}

int display_playlist(PlayList *list, int max_list_size, int indent,
                    int *chosenSong, int *chosenNodeId, bool reset)
{
        AppState *state = get_app_state();
        int termWidth, termHeight;
        get_term_size(&termWidth, &termHeight);

        UISettings *ui = &(state->uiSettings);

        int foundAt = -1;

        Node *startNode = NULL;
        if (list != NULL)
                startNode = determine_start_node(list->head, &foundAt, list->count);

        ensure_chosen_song_within_limits(chosenSong, list);

        start_iter = determine_playlist_start(start_iter, foundAt, max_list_size, chosenSong,
                                           reset, audio_data.endOfListReached);

        move_start_node_into_position(foundAt, &startNode);

        int printedRows = display_playlist_items(startNode, start_iter, max_list_size, termWidth,
                                               indent, *chosenSong, chosenNodeId, ui);

        while (printedRows <= max_list_size)
        {
                clear_line();
                printf("\n");
                printedRows++;
        }

        return printedRows;
}

void set_end_of_list_reached(void)
{
        AppState *state = get_app_state();
        PlaybackState *ps = get_playback_state();

        ps->loadedNextSong = false;
        ps->waitingForNext = true;
        audio_data.endOfListReached = true;
        audio_data.currentFileIndex = 0;
        audio_data.restart = true;
        ps->usingSongDataA = false;
        ps->loadingdata.loadA = true;

        clear_current_song();

        playback_cleanup();

        trigger_refresh();

        if (is_repeat_list_enabled())
                repeat_list();
        else
        {
                emit_playback_stopped_mpris();
                emit_metadata_changed("", "", "", "",
                                    "/org/mpris/MediaPlayer2/TrackList/NoTrack",
                                    NULL, 0);
                state->currentView = LIBRARY_VIEW;
                clear_screen();
        }
}
