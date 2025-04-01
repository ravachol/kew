#include "searchradio_ui.h"

/*

searchradio_ui.c

 Radio Search UI functions.

*/

#define MAX_SEARCH_LEN 32
#define MAX_LINE_LENGTH 2048

const char RADIOFAVORITES_FILE[] = "kewradiofavorites";

int numRadioSearchLetters = 0;
int numRadioSearchBytes = 0;

RadioSearchResult *radioSearchResults = NULL;
RadioSearchResult *radioFavorites = NULL;

size_t radioFavoritesCount = 0;
size_t radioFavoritesCapacity = 0;

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

void addRadioResult(RadioSearchResult **radioSearchResults, size_t *radioResultsCount, size_t *radioResultsCapacity,
                    const char *name, const char *url_resolved, const char *country, const char *codec, int bitrate, int votes)
{
        // Only accept MP3 codec
        if (strcmp(codec, "MP3") != 0)
                return;

        // Check for duplicate URL
        for (size_t i = 0; i < *radioResultsCount; i++)
        {
                if (strcmp((*radioSearchResults)[i].url_resolved, url_resolved) == 0)
                {
                        return; // Duplicate found, do not add
                }
        }

        // Resize array if needed
        if (*radioResultsCount >= *radioResultsCapacity)
        {
                *radioResultsCapacity = (*radioResultsCapacity == 0) ? 10 : (*radioResultsCapacity * 2);
                RadioSearchResult *temp = realloc(*radioSearchResults, *radioResultsCapacity * sizeof(RadioSearchResult));
                if (!temp)
                {
                        return; // Memory allocation failed
                }
                *radioSearchResults = temp;
        }

        // Add new radio station
        RadioSearchResult *newEntry = &(*radioSearchResults)[*radioResultsCount];

        strncpy(newEntry->name, name, sizeof(newEntry->name) - 1);
        newEntry->name[sizeof(newEntry->name) - 1] = '\0';

        strncpy(newEntry->url_resolved, url_resolved, sizeof(newEntry->url_resolved) - 1);
        newEntry->url_resolved[sizeof(newEntry->url_resolved) - 1] = '\0';

        strncpy(newEntry->country, country, sizeof(newEntry->country) - 1);
        newEntry->country[sizeof(newEntry->country) - 1] = '\0';

        strncpy(newEntry->codec, codec, sizeof(newEntry->codec) - 1);
        newEntry->codec[sizeof(newEntry->codec) - 1] = '\0';

        // Copy bitrate and votes
        newEntry->bitrate = bitrate;
        newEntry->votes = votes;

        (*radioResultsCount)++;
}

void removeFromFavorites(RadioSearchResult *radioSearchResults, size_t *count, const RadioSearchResult *result)
{
        if (radioSearchResults == NULL || result == NULL || *count == 0)
        {
                return;
        }

        size_t index = -1;

        // Find the index
        for (size_t i = 0; i < *count; i++)
        {
                if (strcmp(radioSearchResults[i].name, result->name) == 0 &&
                    strcmp(radioSearchResults[i].url_resolved, result->url_resolved) == 0 &&
                    strcmp(radioSearchResults[i].country, result->country) == 0 &&
                    strcmp(radioSearchResults[i].codec, result->codec) == 0 &&
                    radioSearchResults[i].bitrate == result->bitrate &&
                    radioSearchResults[i].votes == result->votes)
                {
                        index = i;
                        break;
                }
        }

        if (index == (size_t)-1)
        {
                return;
        }

        for (size_t i = index; i < *count - 1; i++)
        {
                radioSearchResults[i] = radioSearchResults[i + 1];
        }

        (*count)--;
}

void removeFromRadioFavorites(RadioSearchResult *result)
{
        removeFromFavorites(radioFavorites, &radioFavoritesCount, result);
}

void addToRadioFavorites(RadioSearchResult *result)
{
        addRadioResult(&radioFavorites, &radioFavoritesCount, &radioFavoritesCapacity,
                       result->name, result->url_resolved, result->country, result->codec, result->bitrate, result->votes);
}

// Callback function to collect results
void collectRadioResult(const char *name, const char *url_resolved, const char *country, const char *codec, const int bitrate, const int votes)
{
        addRadioResult(&radioSearchResults, &radioResultsCount, &radioResultsCapacity, name, url_resolved, country, codec, bitrate, votes);
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

void sortRadioResults(RadioSearchResult *radioSearchResults, size_t radioResultsCount)
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

bool hasRadioSearchText()
{
        return (numRadioSearchLetters != 0);
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

int displayRadioSearchResults(RadioSearchResult *radioSearchResults, size_t radioResultsCount, int maxListSize, int indent, int *chosenRow, int startSearchIter, UISettings *ui)
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);

        int maxNameWidth = term_w - indent - 5;
        char name[maxNameWidth + 1];
        int printedRows = 0;

        RadioSearchResult *currentlyPlayingStation = getCurrentPlayingRadioStation();

        sortRadioResults(radioSearchResults, radioResultsCount);

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

        bool isFavorite = false;

        // Print the sorted results
        for (size_t i = startSearchIter; i < radioResultsCount; i++)
        {
                if ((int)i >= maxListSize + startSearchIter - 1)
                        break;

                setDefaultTextColor();

                printBlankSpaces(indent);

                if (radioSearchResults != radioFavorites)
                {
                        for (size_t j = 0; j < radioFavoritesCount; j++)
                        {
                                isFavorite = strcmp(radioSearchResults[i].url_resolved, radioFavorites[j].url_resolved) == 0;
                                if (isFavorite)
                                        break;
                        }
                }

                if (*chosenRow == (int)i)
                {
                        currentRadioSearchEntry = &radioSearchResults[i];

                        if (currentlyPlayingStation != NULL &&
                            strcmp(radioSearchResults[i].url_resolved, currentlyPlayingStation->url_resolved) == 0)
                        {
                                setTextColor(ui->enqueuedColor);

                                printf("\x1b[7m * ");
                        }
                        else if (isFavorite)
                        {
                                setTextColor(ui->enqueuedColor);
                                printf("  \x1b[7m ");
                        }
                        else
                        {
                                printf("  \x1b[7m ");
                        }
                }
                else
                {
                        if (currentlyPlayingStation != NULL &&
                            strcmp(radioSearchResults[i].url_resolved, currentlyPlayingStation->url_resolved) == 0)

                        {
                                setTextColor(ui->enqueuedColor);
                                printf(" * ");
                        }
                        else if (isFavorite)
                        {
                                setTextColor(ui->enqueuedColor);
                                printf("   ");
                        }
                        else
                                printf("   ");
                }

                name[0] = '\0';

                char buffer[20];
                snprintf(name, maxNameWidth + 1, "%d. %s %s %s",
                         (int)i + 1,
                         radioSearchResults[i].name,
                         radioSearchResults[i].bitrate == 0 ? "" : snprintf(buffer, sizeof(buffer), "[%d]", radioSearchResults[i].bitrate) > 0 ? buffer
                                                                                                                                               : "",
                         radioSearchResults[i].country);

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

        if (numRadioSearchLetters > 0)
                displayRadioSearchResults(radioSearchResults, radioResultsCount, maxListSize, indent, chosenRow, startSearchIter, ui);
        else
                displayRadioSearchResults(radioFavorites, radioFavoritesCount, maxListSize, indent, chosenRow, startSearchIter, ui);

        return 0;
}

void stripTripleColon(char *str)
{
        char *src = str;
        char *dst = str;

        while (*src)
        {
                // Look for triple colons ":::"
                if (src[0] == ':' && src[1] == ':' && src[2] == ':')
                {
                        src += 3;
                }
                else
                {
                        *dst++ = *src++;
                }
        }

        *dst = '\0';
}

void writeRadioResultsToFile(RadioSearchResult *radioFavorites, size_t count, FILE *file)
{
        if (radioFavorites == NULL || file == NULL || count == 0)
        {
                return;
        }

        for (size_t i = 0; i < count; i++)
        {
                stripTripleColon(radioFavorites[i].name);
                stripTripleColon(radioFavorites[i].url_resolved);
                stripTripleColon(radioFavorites[i].country);
                stripTripleColon(radioFavorites[i].codec);

                char line[MAX_LINE_LENGTH];
                int written = snprintf(line, sizeof(line), "%s:::%s:::%s:::%s:::%d:::%d\n",
                                       radioFavorites[i].name,
                                       radioFavorites[i].url_resolved,
                                       radioFavorites[i].country,
                                       radioFavorites[i].codec,
                                       radioFavorites[i].bitrate,
                                       radioFavorites[i].votes);

                // If the line is too long, skip it
                if (written >= MAX_LINE_LENGTH)
                {
                        continue;
                }

                // Write the formatted line to the file
                fprintf(file, "%s", line);
        }
}
void writeRadioFavorites(RadioSearchResult *favorites, const char *filename, size_t count)
{
        FILE *file = fopen(filename, "w");
        if (!file)
        {
                return;
        }

        writeRadioResultsToFile(favorites, count, file);
        fclose(file);
}

char *getRadioFavoritesFilePath(void)
{
        return getFilePath(RADIOFAVORITES_FILE);
}

void freeAndwriteRadioFavorites(void)
{
        if (radioFavorites == NULL)
                return;

        char *filepath = getRadioFavoritesFilePath();

        writeRadioFavorites(radioFavorites, filepath, radioFavoritesCount);

        if (radioFavorites != NULL)
        {
                free(radioFavorites);
                radioFavorites = NULL;
        }

        free(filepath);
}

void splitLineByTripleColon(char *line, char **tokens)
{
        size_t tokenIndex = 0;
        char *start = line;
        size_t len = strlen(line);

        for (size_t i = 0; i < len; i++)
        {

                if (i + 2 < len && line[i] == ':' && line[i + 1] == ':' && line[i + 2] == ':')
                {

                        line[i] = '\0';
                        tokens[tokenIndex++] = start;
                        i += 2;
                        start = &line[i + 1];
                }
        }

        tokens[tokenIndex] = start;
}

RadioSearchResult *reconstructRadioFavoritesFromFile(const char *filename, size_t *count, size_t *capacity)
{
        FILE *file = fopen(filename, "r");

        if (file == NULL || count == NULL || capacity == NULL)
        {
                return NULL;
        }

        *capacity = 10;

        // Allocate initial memory
        RadioSearchResult *radioFavorites = malloc(*capacity * sizeof(RadioSearchResult));
        if (radioFavorites == NULL)
        {
                return NULL;
        }

        char line[MAX_LINE_LENGTH];
        while (fgets(line, sizeof(line), file))
        {
                line[strcspn(line, "\n")] = 0;

                char *tokens[6] = {0};
                splitLineByTripleColon(line, tokens);

                if (tokens[0] == NULL || tokens[1] == NULL || tokens[2] == NULL || tokens[3] == NULL || tokens[4] == NULL || tokens[5] == NULL)
                {
                        continue;
                }

                strncpy(radioFavorites[*count].name, tokens[0], sizeof(radioFavorites[*count].name) - 1);
                strncpy(radioFavorites[*count].url_resolved, tokens[1], sizeof(radioFavorites[*count].url_resolved) - 1);
                strncpy(radioFavorites[*count].country, tokens[2], sizeof(radioFavorites[*count].country) - 1);
                strncpy(radioFavorites[*count].codec, tokens[3], sizeof(radioFavorites[*count].codec) - 1);
                radioFavorites[*count].bitrate = atoi(tokens[4]);
                radioFavorites[*count].votes = atoi(tokens[5]);

                (*count)++;

                // Resize the array if needed
                if (*count >= *capacity)
                {
                        *capacity *= 2;
                        RadioSearchResult *temp = realloc(radioFavorites, *capacity * sizeof(RadioSearchResult));
                        if (!temp)
                        {
                                free(radioFavorites);
                                fclose(file);
                                return NULL;
                        }
                        radioFavorites = temp;
                }
        }

        fclose(file);
        return radioFavorites;
}

void createRadioFavorites(void)
{
        char *favoritesFilepath = getRadioFavoritesFilePath();
        radioFavorites = reconstructRadioFavoritesFromFile(favoritesFilepath, &radioFavoritesCount, &radioFavoritesCapacity);
        free(favoritesFilepath);
}
