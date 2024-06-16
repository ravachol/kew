#include <stdbool.h>
#include "directorytree.h"
#include "term.h"
#include "common_ui.h"

extern bool newUndisplayedSearch;

int displaySearch(int maxListSize, int indent, int *chosenRow);
int addToSearchText(const char *str);
int removeFromSearchText();
void fuzzySearch(FileSystemEntry *root, int threshold);
void freeSearchResults();
FileSystemEntry *getCurrentSearchEntry();
