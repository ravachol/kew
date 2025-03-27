#include "searchradio_ui.h"

/*

searchradio_ui.c

 Radio Search UI functions.

*/

#define MAX_SEARCH_LEN 32

int numRadioSearchLetters = 0;
int numRadioSearchBytes = 0;

RadioSearchResult *radioSearchResults = NULL;
size_t radioResultsCount = 0;
size_t radioResultsCapacity = 0;

int minRadioSearchLetters = 1;
RadioSearchResult *currentRadioSearchEntry = NULL;

RadioSearchResult *playingRadioSearchEntry = NULL;

RadioSearchResult *currentPlayingRadioStation = NULL;

char radioSearchText[MAX_SEARCH_LEN * 4 + 1]; // unicode can be 4 characters

RadioSearchResult *getCurrentRadioSearchEntry(void)
{
        return currentRadioSearchEntry;
}

int getRadioSearchResultsCount(void)
{
        return radioResultsCount;
}

void addRadioResult(const char *name, const char *url_resolved, const char *country, const char *codec, const int bitrate, const int votes)
{
        if (strcmp(codec, "MP3") != 0)
                return;

        for (size_t i = 0; i < radioResultsCount; i++)
        {
                if (strcmp(radioSearchResults[i].url_resolved, url_resolved) == 0)
                {
                        return; // Duplicate
                }
        }

        if (radioResultsCount >= radioResultsCapacity)
        {
                radioResultsCapacity = radioResultsCapacity == 0 ? 10 : radioResultsCapacity * 2;
                radioSearchResults = realloc(radioSearchResults, radioResultsCapacity * sizeof(RadioSearchResult));
        }

        strncpy(radioSearchResults[radioResultsCount].name, name, sizeof(radioSearchResults[radioResultsCount].name) - 1);
        radioSearchResults[radioResultsCount].name[sizeof(radioSearchResults[radioResultsCount].name) - 1] = '\0';

        strncpy(radioSearchResults[radioResultsCount].url_resolved, url_resolved, sizeof(radioSearchResults[radioResultsCount].url_resolved) - 1);
        radioSearchResults[radioResultsCount].url_resolved[sizeof(radioSearchResults[radioResultsCount].url_resolved) - 1] = '\0';

        strncpy(radioSearchResults[radioResultsCount].country, country, sizeof(radioSearchResults[radioResultsCount].country) - 1);
        radioSearchResults[radioResultsCount].country[sizeof(radioSearchResults[radioResultsCount].country) - 1] = '\0';

        strncpy(radioSearchResults[radioResultsCount].codec, codec, sizeof(radioSearchResults[radioResultsCount].codec) - 1);
        radioSearchResults[radioResultsCount].codec[sizeof(radioSearchResults[radioResultsCount].codec) - 1] = '\0';

        radioSearchResults[radioResultsCount].bitrate = bitrate;

        radioSearchResults[radioResultsCount].votes = votes;

        radioResultsCount++;
}

// Callback function to collect results
void collectRadioResult(const char *name, const char *url_resolved, const char *country, const char *codec, const int bitrate, const int votes)
{
        addRadioResult(name, url_resolved, country, codec, bitrate, votes);
}

// Free allocated memory from previous search
void freeRadioSearchResults(void)
{
        if (radioSearchResults != NULL)
        {
                free(radioSearchResults);
                radioSearchResults = NULL;
        }
        if (currentRadioSearchEntry != NULL)
                currentRadioSearchEntry = NULL;
        radioResultsCapacity = 0;
        radioResultsCount = 0;
}

void radioSearch()
{
        freeRadioSearchResults();

        if (numRadioSearchLetters > minRadioSearchLetters)
        {
                 if (internetRadioSearch(radioSearchText, collectRadioResult) < 0)
                 {
                        setErrorMessage("Radio database unavailable.");
                 }
        }
        refresh = true;
}

int compareRadioResults(const void *a, const void *b)
{
        RadioSearchResult *resultA = (RadioSearchResult *)a;
        RadioSearchResult *resultB = (RadioSearchResult *)b;
        return resultB->votes - resultA->votes;
}

void sortRadioResults(void)
{
        qsort(radioSearchResults, radioResultsCount, sizeof(RadioSearchResult), compareRadioResults);
}

int displayRadioSearchBox(int indent, UISettings *ui)
{
        if (ui->useConfigColors)
                setTextColor(ui->mainColor);
        else
                setColor(ui);

        printBlankSpaces(indent);
        printf(" [Radio Search]: ");
        setDefaultTextColor();
        // Save cursor position
        printf("%s", radioSearchText);
        printf("\033[s");
        printf("█\n");

        return 0;
}

int addToRadioSearchText(const char *str)
{
        if (str == NULL)
        {
                return -1;
        }

        size_t len = strnlen(str, MAX_SEARCH_LEN);

        // Check if the string can fit into the search text buffer
        if (numRadioSearchLetters + len > MAX_SEARCH_LEN)
        {
                return 0; // Not enough space
        }

        // Restore cursor position
        printf("\033[u");

        // Print the string
        printf("%s", str);

        // Save cursor position
        printf("\033[s");

        printf("█\n");

        // Add the string to the search text buffer
        for (size_t i = 0; i < len; i++)
        {
                radioSearchText[numRadioSearchBytes++] = str[i];
        }

        radioSearchText[numRadioSearchBytes] = '\0'; // Null-terminate the buffer

        numRadioSearchLetters++;

        return 0;
}

// Determine the number of bytes in the last UTF-8 character
int getLastRadioCharBytes(const char *str, int len)
{
        if (len == 0)
                return 0;

        int i = len - 1;
        while (i >= 0 && (str[i] & 0xC0) == 0x80)
        {
                i--;
        }
        return len - i;
}

// Remove the preceding character from the search text
int removeFromRadioSearchText(void)
{
        if (numRadioSearchLetters == 0)
                return 0;

        // Determine the number of bytes to remove for the last character
        int lastCharBytes = getLastRadioCharBytes(radioSearchText, numRadioSearchBytes);
        if (lastCharBytes == 0)
                return 0;

        // Restore cursor position
        printf("\033[u");

        // Move cursor back one step
        printf("\033[D");

        // Overwrite the character with spaces
        for (int i = 0; i < lastCharBytes; i++)
        {
                printf(" ");
        }

        // Move cursor back again to the original position
        for (int i = 0; i < lastCharBytes; i++)
        {
                printf("\033[D");
        }

        // Save cursor position
        printf("\033[s");

        // Print a block character to represent the cursor
        printf("█");

        // Clear the end of the line
        printf("\033[K");

        fflush(stdout);

        // Remove the character from the buffer
        numRadioSearchBytes -= lastCharBytes;
        radioSearchText[numRadioSearchBytes] = '\0';

        numRadioSearchLetters--;

        return 0;
}

int displayRadioSearchResults(int maxListSize, int indent, int *chosenRow, int startSearchIter, UISettings *ui)
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);

        int maxNameWidth = term_w - indent - 5;
        char name[maxNameWidth + 1];
        int printedRows = 0;

        RadioSearchResult *currentlyPlayingStation = getCurrentPlayingRadioStation();

        sortRadioResults();

        currentRadioSearchEntry = &radioSearchResults[0];

        if (*chosenRow >= (int)radioResultsCount - 1)
        {
                *chosenRow = radioResultsCount - 1;
        }

        if (startSearchIter < 0)
                startSearchIter = 0;

        if (*chosenRow > startSearchIter + round(maxListSize / 2))
        {
                startSearchIter = *chosenRow - round(maxListSize / 2) + 1;
        }

        if (*chosenRow < startSearchIter)
                startSearchIter = *chosenRow;

        if (*chosenRow < 0)
                startSearchIter = *chosenRow = 0;

        printf("\n");
        printedRows++;

        // Print the sorted results
        for (size_t i = startSearchIter; i < radioResultsCount; i++)
        {
                if (numRadioSearchLetters < minRadioSearchLetters)
                        break;

                if ((int)i >= maxListSize + startSearchIter - 1)
                        break;

                setDefaultTextColor();

                printBlankSpaces(indent);

                if (*chosenRow == (int)i)
                {
                        currentRadioSearchEntry = &radioSearchResults[i];

                        if (currentlyPlayingStation != NULL &&
                            strcmp(radioSearchResults[i].url_resolved,  currentlyPlayingStation->url_resolved) == 0)
                        {
                                if (ui->useConfigColors)
                                        setTextColor(ui->enqueuedColor);
                                else
                                        setColor(ui);
                                printf("\x1b[7m * ");
                        }
                        else
                        {
                                printf("  \x1b[7m ");
                        }
                }
                else
                {
                        if (currentlyPlayingStation != NULL &&
                            strcmp(radioSearchResults[i].url_resolved,  currentlyPlayingStation->url_resolved) == 0)

                        {
                                if (ui->useConfigColors)
                                        setTextColor(ui->enqueuedColor);
                                else
                                        setColor(ui);
                                printf(" * ");
                        }
                        else
                                printf("   ");
                }

                name[0] = '\0';

                snprintf(name, maxNameWidth + 1, "%d. %s (%s)", (int)i + 1, radioSearchResults[i].name, radioSearchResults[i].country);

                printf("%s\n", name);
                printedRows++;
        }

        while (printedRows < maxListSize)
        {
                printf("\n");
                printedRows++;
        }

        return 0;
}

int displayRadioSearch(int maxListSize, int indent, int *chosenRow, int startSearchIter, UISettings *ui)
{
        displayRadioSearchBox(indent, ui);
        displayRadioSearchResults(maxListSize, indent, chosenRow, startSearchIter, ui);

        return 0;
}
