#include "playlist_ui.h"

int startIter = 0;

void getTerminalSize(int *width, int *height)
{
        getTermSize(width, height);
}

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
        c_strcpy(filePath, sizeof(filePath), node->song.filePath);
        char *lastSlash = strrchr(filePath, '/');
        char *lastDot = strrchr(filePath, '.');

        if (lastSlash != NULL && lastDot != NULL && lastDot > lastSlash)
        {
                int nameLength = lastDot - lastSlash - 1;
                nameLength = (nameLength < bufferSize - 1) ? nameLength : bufferSize - 1;

                strncpy(buffer, lastSlash + 1, nameLength);
                buffer[nameLength] = '\0';
                removeUnneededChars(buffer);
                shortenString(buffer, shortenAmount);
                trim(buffer);
        }
        else
        {
                buffer[0] = '\0';
        }
}

int displayPlaylistItems(Node *startNode, int startIter, int maxListSize, int termWidth, int indent, bool startFromCurrent, int chosenSong, int *chosenNodeId)
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
                        setDefaultTextColor();
                        printBlankSpaces(indent);

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

                        if (startFromCurrent)
                        {
                                printf(" %d. %s \n", i + 1, buffer);
                        }
                        else
                        {
                                printf(" %d. %s \n", numPrintedRows + 1, buffer);
                        }

                        numPrintedRows++;
                }

                node = node->next;
        }

        free(buffer);
        return numPrintedRows;
}

int displayPlaylist(PlayList *list, int maxListSize, int indent, int chosenSong, int *chosenNodeId, bool reset)
{
        int termWidth, termHeight;
        getTerminalSize(&termWidth, &termHeight);

        int foundAt = -1;
        bool startFromCurrent = false;
        Node *startNode = determineStartNode(list->head, &foundAt, &startFromCurrent, list->count);

        // Determine chosen song
        if (chosenSong >= originalPlaylist->count)
        {
                chosenSong = originalPlaylist->count - 1;
        }

        if (chosenSong < 0)
        {
                chosenSong = 0;
        } 

        // Determine where to start iterating
        startIter = (startFromCurrent && (foundAt < startIter || foundAt > startIter + maxListSize)) ? foundAt : startIter;

        int startIter = 0;

        if (chosenSong < startIter)
        {
                startIter = chosenSong;
        }
    
        if (chosenSong > startIter + maxListSize - round(maxListSize / 2))
        {
                startIter = chosenSong - maxListSize + round(maxListSize / 2);
        }

        if (reset && !audioData.endOfListReached)
        {
                startIter = chosenSong = foundAt;
        }
        
        // Go up
        for (int i = foundAt; i > startIter; i--)
        {
                if (i > 0 && startNode->prev != NULL)
                        startNode = startNode->prev;
        }
        
        // Go down
        if (foundAt > -1)
        {
                for (int i = foundAt; i < startIter; i++)
                {
                        if (startNode->next != NULL)
                               startNode = startNode->next;
                }
        }        

        int printedRows = displayPlaylistItems(startNode, startIter, maxListSize, termWidth, indent, startFromCurrent, chosenSong, chosenNodeId);

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
