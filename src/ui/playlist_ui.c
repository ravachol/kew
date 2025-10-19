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

#include "data/songloader.h"

#include "ops/playlist_ops.h"
#include "sys/mpris.h"
#include "utils/term.h"
#include "utils/utils.h"

#include <math.h>
#include <string.h>

static const int MAX_TERM_WIDTH = 1000;

static int startIter = 0;
static int previousChosenSong = 0;
static bool isSameNameAsLastTime = false;

Node *determineStartNode(Node *head, int *foundAt, int listSize)
{
        if (foundAt == NULL)
        {
                return head;
        }

        Node *node = head;
        Node *current = getCurrentSong();
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

void preparePlaylistString(Node *node, char *buffer, int bufferSize)
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

int displayPlaylistItems(Node *startNode, int startIter, int maxListSize,
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

        for (int i = startIter; node != NULL && i < startIter + maxListSize;
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
                        rowColor = getGradientColor(rgbRowNum, i - startIter,
                                                    maxListSize,
                                                    maxListSize / 2, 0.7f);

                if (!(rgbTitle.r == defaultColor &&
                      rgbTitle.g == defaultColor &&
                      rgbTitle.b == defaultColor))
                        rowColor2 = getGradientColor(rgbTitle, i - startIter,
                                                    maxListSize,
                                                    maxListSize / 2, 0.7f);

                preparePlaylistString(node, buffer, NAME_MAX);

                if (buffer[0] != '\0')
                {
                        if (ui->colorMode == COLOR_MODE_ALBUM || ui->colorMode == COLOR_MODE_THEME)
                                applyColor(COLOR_MODE_ALBUM, ui->theme.playlist_rownum,
                                        rowColor);
                        else
                                applyColor(ui->colorMode, ui->theme.playlist_rownum,
                                        rowColor);

                        clearLine();
                        printBlankSpaces(indent);
                        printf("   %d. ", i + 1);

                        if (ui->colorMode == COLOR_MODE_ALBUM || ui->colorMode == COLOR_MODE_THEME)
                                applyColor(COLOR_MODE_ALBUM, ui->theme.playlist_title,
                                        rowColor2);
                        else
                                applyColor(ui->colorMode, ui->theme.playlist_title,
                                        ui->defaultColorRGB);

                        isSameNameAsLastTime =
                            (previousChosenSong == chosenSong);

                        if (!isSameNameAsLastTime)
                        {
                                resetNameScroll();
                        }

                        filename[0] = '\0';

                        if (i == chosenSong)
                        {
                                previousChosenSong = chosenSong;

                                *chosenNodeId = node->id;

                                processNameScroll(buffer, filename,
                                                  maxNameWidth,
                                                  isSameNameAsLastTime);

                                inverseText();
                        }
                        else
                        {
                                processName(buffer, filename, maxNameWidth,
                                            true, true);
                        }

                        Node *current = getCurrentSong();

                        if (current != NULL && current->id == node->id)
                                applyColor(ui->colorMode,
                                           ui->theme.playlist_playing,
                                           rowColor);

                        if (i + 1 < 10)
                                printf(" ");

                        if (current != NULL && current->id == node->id &&
                            i == chosenSong)
                        {
                                inverseText();
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

                resetColor();
        }

        free(buffer);
        free(filename);

        return numPrintedRows;
}

void ensureChosenSongWithinLimits(int *chosenSong, PlayList *list)
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

int determinePlaylistStart(int previousStartIter, int foundAt, int maxListSize,
                           int *chosenSong, bool reset, bool endOfListReached)
{
        int startIter = 0;

        startIter = (foundAt > -1 && (foundAt > startIter + maxListSize))
                        ? foundAt
                        : startIter;

        if (previousStartIter <= foundAt &&
            foundAt < previousStartIter + maxListSize)
                startIter = previousStartIter;

        if (*chosenSong < startIter)
        {
                startIter = *chosenSong;
        }

        if (*chosenSong > startIter + maxListSize - floor(maxListSize / 2))
        {
                startIter = *chosenSong - maxListSize + floor(maxListSize / 2);
        }

        if (reset && !endOfListReached)
        {
                if (foundAt > maxListSize)
                        startIter = previousStartIter = *chosenSong = foundAt;
                else
                        startIter = *chosenSong = 0;
        }

        return startIter;
}

void moveStartNodeIntoPosition(int foundAt, Node **startNode)
{
        // Go up to adjust the startNode
        for (int i = foundAt; i > startIter; i--)
        {
                if (i > 0 && (*startNode)->prev != NULL)
                        *startNode = (*startNode)->prev;
        }

        // Go down to adjust the startNode
        for (int i = (foundAt == -1) ? 0 : foundAt; i < startIter; i++)
        {
                if ((*startNode)->next != NULL)
                        *startNode = (*startNode)->next;
        }
}

int displayPlaylist(PlayList *list, int maxListSize, int indent,
                    int *chosenSong, int *chosenNodeId, bool reset)
{
        AppState *state = getAppState();
        int termWidth, termHeight;
        getTermSize(&termWidth, &termHeight);

        UISettings *ui = &(state->uiSettings);

        int foundAt = -1;

        Node *startNode = NULL;
        if (list != NULL)
                startNode = determineStartNode(list->head, &foundAt, list->count);

        ensureChosenSongWithinLimits(chosenSong, list);

        startIter = determinePlaylistStart(startIter, foundAt, maxListSize, chosenSong,
                                           reset, audioData.endOfListReached);

        moveStartNodeIntoPosition(foundAt, &startNode);

        int printedRows = displayPlaylistItems(startNode, startIter, maxListSize, termWidth,
                                               indent, *chosenSong, chosenNodeId, ui);

        while (printedRows <= maxListSize)
        {
                clearLine();
                printf("\n");
                printedRows++;
        }

        return printedRows;
}

void setEndOfListReached(void)
{
        AppState *state = getAppState();
        PlaybackState *ps = getPlaybackState();

        ps->loadedNextSong = false;
        ps->waitingForNext = true;
        audioData.endOfListReached = true;
        audioData.currentFileIndex = 0;
        audioData.restart = true;
        ps->usingSongDataA = false;
        ps->loadingdata.loadA = true;

        clearCurrentSong();

        playbackCleanup();

        triggerRefresh();

        if (isRepeatListEnabled())
                repeatList();
        else
        {
                emitPlaybackStoppedMpris();
                emitMetadataChanged("", "", "", "",
                                    "/org/mpris/MediaPlayer2/TrackList/NoTrack",
                                    NULL, 0);
                state->currentView = LIBRARY_VIEW;
                clearScreen();
        }
}
