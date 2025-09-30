#include "playlist_ui.h"
#include "common_ui.h"
#include "songloader.h"
#include "term.h"
#include "utils.h"
#include <string.h>

/*

playlist_ui.c

 Playlist UI functions.

*/

int startIter = 0;
int previousChosenSong = 0;

#define MAX_TERM_WIDTH 1000

Node *determineStartNode(Node *head, int *foundAt, int listSize)
{
        if (foundAt == NULL)
        {
                return head;
        }

        Node *node = head;
        Node *foundNode = NULL;
        int numSongs = 0;
        *foundAt = -1;

        while (node != NULL && numSongs <= listSize)
        {
                if (currentSong != NULL && currentSong->id == node->id)
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
                size_t nameLength = (size_t)(filePath + len - (lastSlash + 1));

                if (nameLength >= (size_t)bufferSize)
                {
                        nameLength = bufferSize - 1;
                }

                c_strcpy(buffer, lastSlash + 1, nameLength + 1);
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

        int bufferSize = termWidth - indent - 12;
        if (bufferSize <= 0 || bufferSize > MAXPATHLEN)
                return 0;

        PixelData rowColor = {defaultColor, defaultColor, defaultColor};

        char *buffer = malloc(bufferSize + 1);
        if (!buffer)
                return 0;

        char *filename = malloc(bufferSize + 1);
        if (!filename)
        {
                free(buffer);
                return 0;
        }

        for (int i = startIter; node != NULL && i < startIter + maxListSize;
             i++)
        {
                if (!(ui->color.r == defaultColor &&
                      ui->color.g == defaultColor &&
                      ui->color.b == defaultColor))
                        rowColor = getGradientColor(ui->color, i - startIter,
                                                    maxListSize,
                                                    maxListSize / 2, 0.7f);

                preparePlaylistString(node, buffer, MAXPATHLEN);

                if (buffer[0] != '\0')
                {
                        if (ui->useConfigColors)
                                setTextColor(ui->artistColor);
                        else
                                setColorAndWeight(0, rowColor,
                                                  ui->useConfigColors);

                        printBlankSpaces(indent);

                        printf("   %d. ", i + 1);

                        setDefaultTextColor();

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

                                processNameScroll(buffer, filename, bufferSize,
                                                  isSameNameAsLastTime);

                                printf("\x1b[7m");
                        }
                        else
                        {
                                processName(buffer, filename, bufferSize, true,
                                            true);
                        }

                        if (i + 1 < 10)
                                printf(" ");

                        if (currentSong != NULL && currentSong->id == node->id)
                        {
                                printf("\e[4m\e[1m");
                        }

                        printf("%s\n", filename);

                        numPrintedRows++;
                }

                node = node->next;
        }

        free(buffer);
        free(filename);

        return numPrintedRows;
}

int displayPlaylist(PlayList *list, int maxListSize, int indent,
                    int *chosenSong, int *chosenNodeId, bool reset,
                    AppState *state)
{
        int termWidth, termHeight;
        getTermSize(&termWidth, &termHeight);

        UISettings *ui = &(state->uiSettings);

        int foundAt = -1;

        Node *startNode = determineStartNode(list->head, &foundAt, list->count);

        // Determine chosen song
        if (*chosenSong >= list->count)
        {
                *chosenSong = list->count - 1;
        }

        if (*chosenSong < 0)
        {
                *chosenSong = 0;
        }

        int startIter = 0;

        // Determine where to start iterating
        startIter = (foundAt > -1 && (foundAt > startIter + maxListSize))
                        ? foundAt
                        : startIter;

        if (*chosenSong < startIter)
        {
                startIter = *chosenSong;
        }

        if (*chosenSong > startIter + maxListSize - round(maxListSize / 2))
        {
                startIter = *chosenSong - maxListSize + round(maxListSize / 2);
        }

        if (reset && !audioData.endOfListReached)
        {
                if (foundAt > maxListSize)
                        startIter = *chosenSong = foundAt;
                else
                        startIter = *chosenSong = 0;
        }

        // Go up to find the starting node
        for (int i = foundAt; i > startIter; i--)
        {
                if (i > 0 && startNode->prev != NULL)
                        startNode = startNode->prev;
        }

        // Go down to adjust the startNode
        for (int i = (foundAt == -1) ? 0 : foundAt; i < startIter; i++)
        {
                if (startNode->next != NULL)
                        startNode = startNode->next;
        }

        int printedRows =
            displayPlaylistItems(startNode, startIter, maxListSize, termWidth,
                                 indent, *chosenSong, chosenNodeId, ui);

        while (printedRows < maxListSize)
        {
                printf("\n");
                printedRows++;
        }

        return printedRows;
}
