#include "playlist_ui.h"

/*

playlist_ui.c

 Playlist UI functions.

*/

int startIter = 0;

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

void preparePlaylistString(Node *node, char *buffer, int bufferSize, int shortenAmount)
{
        if (node == NULL || buffer == NULL)
        {
                buffer[0] = '\0';
                return;
        }

        char filePath[MAXPATHLEN];
        c_strcpy(filePath, node->song.filePath, sizeof(filePath));
        char *lastSlash = strrchr(filePath, '/');
        char *lastDot = strrchr(filePath, '.');

        if (lastSlash != NULL && lastDot != NULL && lastDot > lastSlash)
        {
                int nameLength = lastDot - lastSlash - 1;
                nameLength = (nameLength < bufferSize - 1) ? nameLength : bufferSize - 1;

                c_strcpy(buffer, lastSlash + 1, nameLength + 1);
                buffer[nameLength] = '\0';
                removeUnneededChars(buffer, nameLength);
                shortenString(buffer, shortenAmount);
                trim(buffer, MAXPATHLEN);
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

        char *buffer = (char *)malloc(MAXPATHLEN * sizeof(char));

        if (!buffer)
        {
                return 0;
        }

        for (int i = startIter; node != NULL && i < startIter + maxListSize; i++)
        {
                preparePlaylistString(node, buffer, MAXPATHLEN, termWidth - indent - 10);
                if (buffer[0] != '\0')
                {
                        if (ui->useConfigColors)
                                setTextColor(ui->artistColor);
                        else
                                setColor(ui);

                        printBlankSpaces(indent);

                        printf(" %d. ", i + 1);

                        setDefaultTextColor();

                        if (i == chosenSong)
                        {
                                *chosenNodeId = node->id;

                                printf("\x1b[7m");
                        }

                        if (currentSong != NULL && currentSong->id == node->id)
                        {
                                printf("\e[1m\e[39m");
                        }

                        if (i + 1 < 10)
                                printf(" ");

                        printf("%s \n", buffer);

                        numPrintedRows++;
                }

                node = node->next;
        }

        free(buffer);
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

        if (printedRows > 1)
        {
                while (printedRows < maxListSize)
                {
                        printf("\n");
                        printedRows++;
                }
        }

        return printedRows;
}
