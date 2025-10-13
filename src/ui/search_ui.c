/**
 * @file search_ui.[c]
 * @brief Search interface for tracks and artists.
 *
 * Provides UI and logic for querying the music library, filtering results,
 * and adding songs to playlists from search results.
 */

#include "search_ui.h"

#include "common/appstate.h"
#include "common/common.h"
#include "common_ui.h"

#include "data/playlist.h"

#include "utils/term.h"
#include "utils/utils.h"

#include <math.h>
#include <stdbool.h>

#define MAX_SEARCH_LEN 32

typedef struct SearchResult
{
        FileSystemEntry *entry;
        struct FileSystemEntry *parent;
        int distance;
        int groupDistance;
} SearchResult;

// Global variables to store results
static SearchResult *results = NULL;
size_t resultsCount = 0;
size_t resultsCapacity = 0;
size_t terminalHeight = 0;
static int numSearchLetters = 0;
static int numSearchBytes = 0;
static int minSearchLetters = 1;
static FileSystemEntry *currentSearchEntry = NULL;

static char searchText[MAX_SEARCH_LEN * 4 + 1]; // Unicode can be 4 characters

FileSystemEntry *getCurrentSearchEntry(void) { return currentSearchEntry; }

int getSearchResultsCount(void) { return resultsCount; }

#define GROW_MARGIN 50

void reallocResults()
{
        if (resultsCount >= resultsCapacity)
        {
                resultsCapacity = resultsCapacity == 0
                                      ? 10 + GROW_MARGIN
                                      : resultsCapacity + GROW_MARGIN;

                results =
                    realloc(results, resultsCapacity * sizeof(SearchResult));
        }
}

void setResultFields(FileSystemEntry *entry, int distance,
                     FileSystemEntry *parent)
{
        results[resultsCount].distance = distance;
        results[resultsCount].entry = entry;
        results[resultsCount].parent = parent;
}

bool isDuplicate(const FileSystemEntry *entry)
{
        for (size_t i = 0; i < resultsCount; i++)
        {
                const FileSystemEntry *other = results[i].entry;

                if (!entry->isDirectory)
                        return false;

                if (entry == other)
                        return true;
        }

        return false;
}

void addResult(FileSystemEntry *entry, int distance)
{
        if (numSearchLetters < minSearchLetters)
                return;

        if (resultsCount > terminalHeight * 10)
                return;

        if (entry->parent == NULL) // Root
                return;

        if (isDuplicate(entry))
                return;

        reallocResults();
        setResultFields(entry, distance, NULL);
        resultsCount++;

        if (entry->isDirectory)
        {
                if (entry->children && entry->parent != NULL &&
                    entry->parent->parent == NULL)
                {
                        FileSystemEntry *child = entry->children;

                        while (child)
                        {
                                if (child->isDirectory)
                                {
                                        if (resultsCount > terminalHeight * 10)
                                                break;

                                        reallocResults();
                                        setResultFields(child, distance, entry);
                                        resultsCount++;
                                }

                                child = child->next;
                        }
                }
        }
}

void collectResult(FileSystemEntry *entry, int distance)
{
        addResult(entry, distance);
}

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

void calculateGroupDistances(void)
{
        for (size_t i = 0; i < resultsCount; i++)
        {
                // Find top-level parent (entry with no parent, or root)
                FileSystemEntry *root = results[i].entry;
                while (root->parent != NULL)
                {
                        root = root->parent;
                }

                // Find if this root appears in results, and use ITS distance
                // Otherwise use minimum distance among descendants
                int minDist = results[i].distance;

                for (size_t j = 0; j < resultsCount; j++)
                {
                        FileSystemEntry *otherRoot = results[j].entry;
                        while (otherRoot->parent != NULL)
                        {
                                otherRoot = otherRoot->parent;
                        }

                        if (otherRoot == root)
                        {
                                if (results[j].entry == root)
                                {
                                        // The root itself is in results - use
                                        // its distance
                                        minDist = results[j].distance;
                                        break;
                                }
                                if (results[j].distance < minDist)
                                {
                                        minDist = results[j].distance + 1;
                                }
                        }
                }

                // If root is in results, use only its distance for grouping
                // Otherwise use the best child distance
                results[i].groupDistance = minDist;
        }
}

static int ancestorCompare(const FileSystemEntry *A, const FileSystemEntry *B)
{
        for (const FileSystemEntry *p = B->parent; p; p = p->parent)
                if (p == A)
                        return -1;
        for (const FileSystemEntry *p = A->parent; p; p = p->parent)
                if (p == B)
                        return 1;
        return 0;
}

int compareResults(const void *a, const void *b)
{
        const SearchResult *A = a;
        const SearchResult *B = b;

        int rel = ancestorCompare(A->entry, B->entry);
        if (rel != 0)
                return rel;

        // Sort by best distance in the top-level group first
        if (A->groupDistance != B->groupDistance)
                return (A->groupDistance < B->groupDistance) ? -1 : 1;

        // If different parents, compare by hierarchy (path)
        if (A->entry->parent != B->entry->parent)
        {
                const FileSystemEntry *pA = A->entry;
                const FileSystemEntry *pB = B->entry;

                // Walk up to same depth
                int depthA = 0, depthB = 0;
                for (const FileSystemEntry *p = pA; p->parent; p = p->parent)
                        depthA++;
                for (const FileSystemEntry *p = pB; p->parent; p = p->parent)
                        depthB++;

                while (depthA > depthB)
                {
                        pA = pA->parent;
                        depthA--;
                }
                while (depthB > depthA)
                {
                        pB = pB->parent;
                        depthB--;
                }

                // Walk up together to find where they diverge
                while (pA->parent != pB->parent)
                {
                        pA = pA->parent;
                        pB = pB->parent;
                }

                // Compare by name at divergence point
                int cmp = strcmp(pA->name, pB->name);
                if (cmp != 0)
                        return cmp;
        }

        // Within same parent: directories first
        if (A->entry->isDirectory != B->entry->isDirectory)
                return A->entry->isDirectory ? -1 : 1;

        // Then by individual distance
        if (A->distance != B->distance)
                return (A->distance < B->distance) ? -1 : 1;

        // Then by name
        int cmp = strcmp(A->entry->name, B->entry->name);
        if (cmp != 0)
                return cmp;

        return (A->entry < B->entry) ? -1 : (A->entry > B->entry);
}

void sortSearchResults(void)
{
        calculateGroupDistances();
        qsort(results, resultsCount, sizeof(SearchResult), compareResults);
}

void fuzzySearch(FileSystemEntry *root, int threshold)
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);

        terminalHeight = term_h;

        freeSearchResults();

        if (numSearchLetters > minSearchLetters)
        {
                fuzzySearchRecursive(root, searchText, threshold,
                                     collectResult);
        }

        sortSearchResults();

        triggerRefresh();
}

int displaySearchBox(int indent)
{
        AppState *state = getAppState();
        UISettings *ui = &(state->uiSettings);
        
        applyColor(ui->colorMode, ui->theme.search_label, ui->color);

        clearLine();
        printBlankSpaces(indent);
        printf(" [Search]: ");
        applyColor(ui->colorMode, ui->theme.search_query, ui->defaultColorRGB);
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

        AppState *state = getAppState();
        UISettings *ui = &(state->uiSettings);
        size_t len = strnlen(str, MAX_SEARCH_LEN);

        // Check if the string can fit into the search text buffer
        if (numSearchLetters + len > MAX_SEARCH_LEN)
        {
                return 0; // Not enough space
        }

        applyColor(ui->colorMode, ui->theme.search_label, ui->color);

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

        searchText[numSearchBytes] = '\0'; // Null-terminate the buffer

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

        clearRestOfLine();

        fflush(stdout);

        // Remove the character from the buffer
        numSearchBytes -= lastCharBytes;
        searchText[numSearchBytes] = '\0';

        numSearchLetters--;

        return 0;
}

void applyColorAndFormat(bool isChosen, FileSystemEntry *entry, UISettings *ui,
                         bool isPlaying)
{
        if (isChosen)
        {
                currentSearchEntry = entry;

                if (entry->isEnqueued)
                {
                        applyColor(ui->colorMode,
                                   isPlaying ? ui->theme.search_playing
                                             : ui->theme.search_enqueued,
                                   ui->color);

                        printf("\x1b[7m * ");
                }
                else
                {
                        printf("  \x1b[7m ");
                }
        }
        else
        {
                if (entry->isEnqueued)
                {
                        applyColor(ui->colorMode,
                                   isPlaying ? ui->theme.search_playing
                                             : ui->theme.search_enqueued,
                                   ui->color);

                        printf(" * ");
                }
                else
                        printf("   ");
        }
}

int displaySearchResults(int maxListSize, int indent, int *chosenRow,
                         int startSearchIter)
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);

        int maxNameWidth = term_w - indent - 5;
        char name[maxNameWidth + 1];
        int printedRows = 0;

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

        clearLine();
        printf("\n");
        printedRows++;

        int nameWidth = maxNameWidth;
        int extraIndent = 0;
        AppState *state = getAppState();
        UISettings *ui = &(state->uiSettings);

        for (size_t i = startSearchIter; i < resultsCount; i++)
        {
                if ((int)i >= maxListSize + startSearchIter - 1)
                        break;

                applyColor(ui->colorMode, ui->theme.search_result,
                           ui->defaultColorRGB);

                clearLine();

                // Indent sub dirs
                if (results[i].parent != NULL)
                        extraIndent = 2;
                else if (!results[i].entry->isDirectory)
                        extraIndent = 4;
                else
                        extraIndent = 0;

                nameWidth = maxNameWidth - extraIndent;

                printBlankSpaces(indent + extraIndent);

                bool isChosen = (*chosenRow == (int)i);

                Node *current = getCurrentSong();

                bool isCurrentSong =
                    current != NULL && strcmp(current->song.filePath,
                                              results[i].entry->fullPath) == 0;

                applyColorAndFormat(isChosen, results[i].entry, ui,
                                    isCurrentSong);

                name[0] = '\0';
                if (results[i].entry->isDirectory)
                {
                        snprintf(name, nameWidth + 1, "[%s]",
                                 results[i].entry->name);
                }
                else
                {
                        snprintf(name, nameWidth + 1, "%s",
                                 results[i].entry->name);
                }
                printf("%s\n", name);
                printedRows++;
        }

        applyColor(ui->colorMode, ui->theme.help, ui->defaultColorRGB);

        while (printedRows < maxListSize)
        {
                clearLine();
                printf("\n");
                printedRows++;
        }

        return 0;
}

int displaySearch(int maxListSize, int indent, int *chosenRow,
                  int startSearchIter)
{
        displaySearchBox(indent);
        displaySearchResults(maxListSize, indent, chosenRow, startSearchIter);

        return 0;
}
