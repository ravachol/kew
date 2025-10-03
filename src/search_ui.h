#include "appstate.h"
#include "directorytree.h"

int displaySearch(int maxListSize, int indent, int *chosenRow, int startSearchIter, UISettings *ui);

int addToSearchText(const char *str, UISettings *ui);

int removeFromSearchText(void);

int getSearchResultsCount(void);

void fuzzySearch(FileSystemEntry *root, int threshold);

void freeSearchResults(void);

FileSystemEntry *getCurrentSearchEntry(void);
