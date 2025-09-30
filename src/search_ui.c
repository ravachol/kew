#include "search_ui.h"
#include "common.h"
#include "common_ui.h"
#include "soundcommon.h"
#include "term.h"
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*

search_ui.c

 Search UI functions.

*/

#define MAX_SEARCH_LEN 32
#define MAX_SEARCH_BYTES 109

int numSearchLetters = 0;
int numSearchBytes = 0;

typedef struct SearchResult
{
        FileSystemEntry *entry;
        int distance;
} SearchResult;

// Global variables to store results
SearchResult *results = NULL;
size_t resultsCount = 0;
size_t resultsCapacity = 0;

int minSearchLetters = 1;
FileSystemEntry *currentSearchEntry = NULL;

char searchText[MAX_SEARCH_LEN * 4 + 1]; // Unicode can be 4 characters

FileSystemEntry *getCurrentSearchEntry(void) { return currentSearchEntry; }

int getSearchResultsCount(void) { return resultsCount; }

#define MAX_RESULTS_CAPACITY 100000

// Function to add a result to the global array
int addResult(FileSystemEntry *entry, int distance)
{
        if (resultsCount >= resultsCapacity)
        {
                size_t newCapacity =
                    (resultsCapacity == 0) ? 10 : resultsCapacity * 2;

                if (newCapacity > MAX_RESULTS_CAPACITY ||
                    newCapacity > SIZE_MAX / sizeof(SearchResult))
                {
                        fprintf(stderr, "addResult: capacity overflow\n");
                        return 0;
                }

                SearchResult *newResults =
                    realloc(results, newCapacity * sizeof(SearchResult));
                if (newResults == NULL)
                {
                        fprintf(stderr,
                                "addResult: memory allocation failed\n");
                        return -1;
                }

                results = newResults;
                resultsCapacity = newCapacity;
        }

        results[resultsCount].distance = distance;
        results[resultsCount].entry = entry;
        resultsCount++;

        return 0; // success
}

// Callback function to collect results
void collectResult(FileSystemEntry *entry, int distance)
{
        if (!addResult(entry, distance))
        {
                printf("Memory allocation error.\n");
                exit(1);
        }
}

// Free allocated memory from previous search
void freeSearchResults(void)
{
        if (results != NULL)
        {
                free(results);
                results = NULL;
        }

        if (currentSearchEntry != NULL)
                currentSearchEntry = NULL;

        resultsCapacity = 0;
        resultsCount = 0;
}

void fuzzySearch(FileSystemEntry *root, int threshold)
{
        freeSearchResults();

        if (numSearchLetters > minSearchLetters)
        {
                fuzzySearchRecursive(root, searchText, threshold,
                                     collectResult);
        }
        refresh = true;
}

int compareResults(const void *a, const void *b)
{
    if (a == NULL && b == NULL)
        return 0; // both equal
    if (a == NULL)
        return -1; // NULL is considered less (or greater, your choice)
    if (b == NULL)
        return 1;

    SearchResult *resultA = (SearchResult *)a;
    SearchResult *resultB = (SearchResult *)b;

    return resultA->distance - resultB->distance;
}

void sortResults(void)
{
        qsort(results, resultsCount, sizeof(SearchResult), compareResults);
}

int displaySearchBox(int indent, UISettings *ui)
{
        if (ui->useConfigColors)
                setTextColor(ui->mainColor);
        else
                setColor(ui);

        printBlankSpaces(indent);
        printf(" [Search]: ");
        setDefaultTextColor();
        // Save cursor position
        printf("%s", searchText);
        printf("\033[s");
        printf("█\n");

        return 0;
}

bool isValidUtf8CharStart(unsigned char byte)
{
        return (byte & 0xC0) != 0x80; // Not a continuation byte
}

int addToSearchText(const char *str)
{
        if (str == NULL)
        {
                return -1;
        }

        size_t strLen = strnlen(str, MAX_SEARCH_LEN);

        // Make sure we have space for every byte + null terminator
        if (strLen == 0 ||
            strLen > (size_t)(MAX_SEARCH_LEN - numSearchBytes - 1))
        {
                return 0; // Not enough space
        }

        if (strLen == 0 ||
            strLen > ((size_t)MAX_SEARCH_BYTES - numSearchBytes - 1))
                return 0;

        if (numSearchLetters > MAX_SEARCH_LEN)
                return 0;

        // Count how many actual UTF-8 characters are in the string
        int utf8CharCount = 0;
        for (size_t i = 0; i < strLen; i++)
        {
                if (isValidUtf8CharStart((unsigned char)str[i]))
                {
                        utf8CharCount++;
                }
        }

        // Make sure character limit is also not exceeded
        if (numSearchLetters + utf8CharCount > MAX_SEARCH_LEN)
        {
                return 0;
        }

        // Restore cursor position
        printf("\033[u");

        // Safely print the string
        printf("%.*s", (int)strLen, str);

        // Save cursor position
        printf("\033[s");
        printf("█\n");

        // Copy into buffer
        for (size_t i = 0; i < strLen; i++)
        {
                searchText[numSearchBytes++] = str[i];
        }

        searchText[numSearchBytes] = '\0';
        numSearchLetters += utf8CharCount;

        return 0;
}

/*
 * Return the number of bytes occupied by the last UTF-8 character
 * in `str` of length `len`. Returns 0 for NULL/len <= 0.
 *
 * Behavior on malformed UTF-8:
 *  - If a valid lead byte is found and the available continuation
 *    bytes match the expected length, return that expected length (1..4).
 *  - If a valid lead byte is found but the sequence is truncated
 *    (not enough continuation bytes), return 1 (treat last byte as lone).
 *  - If no lead byte is found within the last 4 bytes, return the
 *    number of bytes available in the suffix (clamped to len).
 */
int getLastCharBytes(const char *str, int len)
{
        if (str == NULL || len <= 0)
        {
                return 0;
        }

        /* Walk backwards to count continuation bytes (0x80..0xBF). */
        int i = len - 1;
        int cont = 0;
        while (i >= 0 && cont < 3 && ((unsigned char)str[i] & 0xC0) == 0x80)
        {
                cont++;
                i--;
        }

        /* If we walked off the front (no lead within 4 bytes), return available
         * suffix length. */
        if (i < 0)
        {
                int bytes = cont + 1; /* suffix includes the last continuation
                                         bytes + the (missing) lead */
                if (bytes > len)
                        bytes = len;
                return bytes;
        }

        unsigned char lead = (unsigned char)str[i];
        int expected;
        if ((lead & 0x80) == 0x00)
        {
                expected = 1;
        }
        else if ((lead & 0xE0) == 0xC0)
        {
                expected = 2;
        }
        else if ((lead & 0xF0) == 0xE0)
        {
                expected = 3;
        }
        else if ((lead & 0xF8) == 0xF0)
        {
                expected = 4;
        }
        else
        {
                /* Invalid lead byte (0x80..0xBF would have been counted as
                 * cont, anything else is invalid). */
                return 1;
        }

        int available = len - i;
        if (available == expected)
        {
                /* Full valid-looking sequence length */
                return expected;
        }

        if (available > expected)
        {
                /* Too many bytes after lead: this shouldn't normally happen,
                 * but clamp to expected. */
                return expected;
        }

        /* Sequence is truncated (available < expected). Treat conservatively as
         * 1 (avoid assuming wide char). */
        return 1;
}

// Remove the preceding character from the search text
int removeFromSearchText(void)
{
        if (numSearchLetters == 0)
                return 0;

        // Determine the number of bytes to remove for the last character
        int lastCharBytes = getLastCharBytes(searchText, numSearchBytes);
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
        numSearchBytes -= lastCharBytes;
        searchText[numSearchBytes] = '\0';

        numSearchLetters--;

        return 0;
}

int displaySearchResults(int maxListSize, int indent, int *chosenRow,
                         int startSearchIter, UISettings *ui)
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);

        int maxNameWidth = term_w - indent - 5;

        if (maxNameWidth < 16)
                maxNameWidth = 16;
        if (maxNameWidth > 1024)
                maxNameWidth = 1024;

        char name[maxNameWidth + 1];
        int printedRows = 0;

        sortResults();

        if (*chosenRow >= (int)resultsCount - 1)
        {
                *chosenRow = resultsCount - 1;
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

        int end = startSearchIter + maxListSize;
        if (end > (int)resultsCount)
                end = resultsCount;

        // Print the sorted results
        for (int i = startSearchIter; i < end; i++)
        {
                if (numSearchLetters < minSearchLetters)
                        break;

                setDefaultTextColor();

                printBlankSpaces(indent);

                if (*chosenRow == (int)i)
                {
                        currentSearchEntry = results[i].entry;

                        if (results[i].entry->isEnqueued)
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
                        if (results[i].entry->isEnqueued)
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
                if (results[i].entry->isDirectory)
                {
                        if (results[i].entry->parent != NULL &&
                            strcmp(results[i].entry->parent->name, "root") != 0)
                                snprintf(name, maxNameWidth + 1, "[%s] (%s)",
                                         results[i].entry->name,
                                         results[i].entry->parent->name);
                        else
                                snprintf(name, maxNameWidth + 1, "[%s]",
                                         results[i].entry->name);
                }
                else
                {
                        if (results[i].entry->parent != NULL &&
                            strcmp(results[i].entry->parent->name, "root") != 0)
                                snprintf(name, maxNameWidth + 1, "%s (%s)",
                                         results[i].entry->name,
                                         results[i].entry->parent->name);
                        else
                                snprintf(name, maxNameWidth + 1, "%s",
                                         results[i].entry->name);
                }
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

int displaySearch(int maxListSize, int indent, int *chosenRow,
                  int startSearchIter, UISettings *ui)
{
        displaySearchBox(indent, ui);
        displaySearchResults(maxListSize, indent, chosenRow, startSearchIter,
                             ui);

        return 0;
}
