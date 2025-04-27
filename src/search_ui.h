#include <stdbool.h>
#include <math.h>
#include "soundcommon.h"
#include "directorytree.h"
#include "term.h"
#include "common_ui.h"
#include "common.h"

int displaySearch(int maxListSize, int indent, int *chosenRow, int startSearchIter, UISettings *ui);

int addToSearchText(const char *str);

int removeFromSearchText(void);

int removeSearchTextUntilSpace(void);

int getSearchResultsCount(void);

void fuzzySearch(FileSystemEntry *root, int threshold);

void freeSearchResults(void);

FileSystemEntry *getCurrentSearchEntry(void);
