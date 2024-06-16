#include "search_ui.h"

#define MAX_SEARCH_LEN 32

int numSearchLetters = 0;
int numSearchBytes = 0;

typedef struct SearchResult
{
        char *term;
        FileSystemEntry *entry;
        int distance;
} SearchResult;

// Global variables to store results
SearchResult *results = NULL;
size_t resultsCount = 0;
size_t resultsCapacity = 0;
bool newUndisplayedSearch = false;
int minSearchLetters = 1;
FileSystemEntry *currentSearchEntry = NULL;

char searchText[MAX_SEARCH_LEN * 4 + 1]; // unicode can be 4 characters

FileSystemEntry *getCurrentSearchEntry()
{
        return currentSearchEntry;
}

// Function to add a result to the global array
void addResult(FileSystemEntry *entry, int distance)
{
        if (resultsCount >= resultsCapacity)
        {
                resultsCapacity = resultsCapacity == 0 ? 10 : resultsCapacity * 2;
                results = realloc(results, resultsCapacity * sizeof(SearchResult));
        }
        results[resultsCount].term = strdup(entry->name);
        results[resultsCount].distance = distance;
        results[resultsCount].entry = entry;
        resultsCount++;
}

// Callback function to collect results
void collectResult(FileSystemEntry *entry, int distance)
{
        addResult(entry, distance);
}

// Free allocated memory from previous search
void freeSearchResults()
{
        for (size_t i = 0; i < resultsCount; i++)
        {
                free(results[i].term);
        }

        if (resultsCount > 0)
        {
                free(results);
                results = NULL;
        }
        resultsCapacity = 0;
        resultsCount = 0;
}

void fuzzySearch(FileSystemEntry *root, int threshold)
{
        freeSearchResults();

        if (numSearchLetters > minSearchLetters)
        {
                fuzzySearchRecursive(root, searchText, threshold, collectResult);
        }
        newUndisplayedSearch = true;
}

int compareResults(const void *a, const void *b)
{
        SearchResult *resultA = (SearchResult *)a;
        SearchResult *resultB = (SearchResult *)b;
        return resultA->distance - resultB->distance;
}

void sortResults()
{
        qsort(results, resultsCount, sizeof(SearchResult), compareResults);
}

int displaySearchBox(int indent)
{
        printBlankSpaces(indent);
        printf(" [Search]: ");
        setDefaultTextColor();   
        // Save cursor position
        printf("%s", searchText);
        printf("\033[s");
        printf("█\n");

        return 0;
}

int addToSearchText(const char *str)
{
        if (str == NULL)
        {
                return -1;
        }

        size_t len = strlen(str);

        // Check if the string can fit into the search text buffer
        if (numSearchLetters + 1 > MAX_SEARCH_LEN)
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
                searchText[numSearchBytes++] = str[i];
        }

        searchText[numSearchBytes + 1] = '\0'; // Null-terminate the buffer

        numSearchLetters++;

        return 0;
}

// Determine the number of bytes in the last UTF-8 character
int getLastCharBytes(const char *str, int len)
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
int removeFromSearchText()
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

int displaySearchResults(int maxListSize, int indent, int *chosenRow)
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);

        int maxNameWidth = term_w - indent - 5;
        char name[maxNameWidth + 1];

        sortResults();

        if (*chosenRow >= (int)resultsCount - 1 || *chosenRow >= maxListSize - 1)
        {
                *chosenRow = MIN(resultsCount, maxListSize) - 1;
        }

        if (*chosenRow < 0)
        {
                *chosenRow = 0;
        }

        printf("\n");

        // Print the sorted results
        for (size_t i = 0; i < resultsCount; i++)
        {
                if (numSearchLetters < minSearchLetters)
                        break;

                if ((int)i >= maxListSize)
                        break;

                setDefaultTextColor();

                printBlankSpaces(indent);

                if (*chosenRow == (int)i)
                {
                        currentSearchEntry = results[i].entry;

                        if (results[i].entry->isEnqueued)
                        {
                                if (useProfileColors)
                                        setTextColor(enqueuedColor);
                                else
                                        setColor();
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
                                if (useProfileColors)
                                        setTextColor(enqueuedColor);
                                else
                                        setColor();
                                printf(" * ");
                        }
                        else
                                printf("   ");
                }


                name[0] = '\0';
                if (results[i].entry->isDirectory)
                {
                        snprintf(name, maxNameWidth + 1, "[%s]", results[i].term);        
                }       
                else
                {         
                        snprintf(name, maxNameWidth + 1, "%s", results[i].term);
                }
                printf("%s\n", name);
        }
        return 0;
}

int displaySearch(int maxListSize, int indent, int *chosenRow)
{     
        displaySearchBox(indent);
        displaySearchResults(maxListSize, indent, chosenRow);

        return 0;
}

// FIXME: page up page down
// FIXME: scroll into view
// FIXME: quit in search view. esc to quit? esc to quit search?
