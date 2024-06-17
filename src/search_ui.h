#include <stdbool.h>
#include <math.h>
#include "directorytree.h"
#include "term.h"
#include "common_ui.h"

extern bool newUndisplayedSearch;

int displaySearch(int maxListSize, int indent, int *chosenRow, int startSearchIter);
int addToSearchText(const char *str);
int removeFromSearchText();
int getSearchResultsCount();
void fuzzySearch(FileSystemEntry *root, int threshold);
void freeSearchResults();
FileSystemEntry *getCurrentSearchEntry();
