#include <stdbool.h>
#include <math.h>
#include "soundcommon.h"
#include "directorytree.h"
#include "term.h"
#include "common_ui.h"
#include "common.h"
#include "soundradio.h"

extern bool newUndisplayedRadioSearch;

int displayRadioSearch(int maxListSize, int indent, int *chosenRow, int startSearchIter, UISettings *ui);

int addToRadioSearchText(const char *str);

int removeFromRadioSearchText(void);

int getRadioSearchResultsCount(void);

bool hasRadioSearchText();

void radioSearch();

void freeRadioSearchResults(void);

void freeAndwriteRadioFavorites(void);

void createRadioFavorites(void);

void addToRadioFavorites(RadioSearchResult *result);

void removeFromRadioFavorites(RadioSearchResult *result);

RadioSearchResult *getCurrentRadioSearchEntry(void);
