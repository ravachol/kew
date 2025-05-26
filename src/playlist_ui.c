#include "common_ui.h"
#include "playlist_ui.h"
#include "songloader.h"
#include "term.h"
#include "utils.h"

/*

playlist_ui.c

 Playlist UI functions.

*/

int startIter = 0;
int previousChosenSong = 0;

Node *determineStartNode(Node *head, int *foundAt, bool *startFromCurrent, int listSize)
{
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

        *startFromCurrent = (*foundAt > -1) ? true : false;
        return foundNode ? foundNode : head;
}

void preparePlaylistString(Node *node, char *buffer, int bufferSize)
{
        if (node == NULL || buffer == NULL)
        {
                buffer[0] = '\0';
                return;
        }

        char filePath[MAXPATHLEN];
        c_strcpy(filePath, node->song.filePath, sizeof(filePath));
        char *lastSlash = strrchr(filePath, '/');
        size_t len = strnlen(filePath, sizeof(filePath));

        if (lastSlash != NULL)
        {
                int nameLength = filePath + len - (lastSlash + 1); // Length of the filename
                nameLength = (nameLength < bufferSize - 1) ? nameLength : bufferSize - 1;

                c_strcpy(buffer, lastSlash + 1, nameLength + 1);
                buffer[nameLength] = '\0';
        }
        else
        {
                buffer[0] = '\0';
        }
}

int displayPlaylistItems(Node *startNode, int startIter, int maxListSize, int termWidth, int indent, int chosenSong, int *chosenNodeId, UISettings *ui)
{
        int numPrintedRows = 0;
        Node *node = startNode;

        int bufferSize = termWidth - indent - 12;

        PixelData rowColor;
        rowColor.r = defaultColor;
        rowColor.g = defaultColor;
        rowColor.b = defaultColor;

        for (int i = startIter; node != NULL && i < startIter + maxListSize; i++)
        {
                if (!(ui->color.r == defaultColor && ui->color.g == defaultColor && ui->color.b == defaultColor))
                        rowColor = getGradientColor(ui->color, i - startIter, maxListSize, maxListSize / 2, 0.7f);

                char *buffer = (char *)malloc(MAXPATHLEN * sizeof(char));
                char *filename = (char *)malloc(MAXPATHLEN * sizeof(char) + 1);

                if (!buffer || !filename)
                {

                        return 0;
                }
                preparePlaylistString(node, buffer, MAXPATHLEN);

                if (buffer[0] != '\0')
                {
                        if (ui->useConfigColors)
                                setTextColor(ui->artistColor);
                        else
                                setColorAndWeight(0, rowColor, ui->useConfigColors);

                        printBlankSpaces(indent);

                        printf("   %d. ", i + 1);

                        setDefaultTextColor();

                        isSameNameAsLastTime = (previousChosenSong == chosenSong);

                        if (!isSameNameAsLastTime)
                        {
                                resetNameScroll();
                        }

                        filename[0] = '\0';

                        if (i == chosenSong)
                        {
                                previousChosenSong = chosenSong;

                                *chosenNodeId = node->id;

                                processNameScroll(buffer, filename, bufferSize, isSameNameAsLastTime);

                                printf("\x1b[7m");
                        }
                        else
                        {
                                processName(buffer, filename, bufferSize);
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

                free(buffer);
                free(filename);

                node = node->next;
        }

        return numPrintedRows;
}

int displayPlaylist(PlayList *list, int maxListSize, int indent, int *chosenSong, int *chosenNodeId, bool reset, AppState *state)
{
        int termWidth, termHeight;
        getTermSize(&termWidth, &termHeight);

        UISettings *ui = &(state->uiSettings);

        int foundAt = -1;
        bool startFromCurrent = false;
        Node *startNode = determineStartNode(list->head, &foundAt, &startFromCurrent, list->count);

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
        startIter = (startFromCurrent && (foundAt < startIter || foundAt > startIter + maxListSize)) ? foundAt : startIter;

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
                startIter = *chosenSong = foundAt;
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

        int printedRows = displayPlaylistItems(startNode, startIter, maxListSize, termWidth, indent, *chosenSong, chosenNodeId, ui);

        while (printedRows < maxListSize)
        {
                printf("\n");
                printedRows++;
        }

        return printedRows;
}
