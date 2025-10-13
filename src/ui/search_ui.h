/**
 * @file search_ui.[h]
 * @brief Search interface for tracks and artists.
 *
 * Provides UI and logic for querying the music library, filtering results,
 * and adding songs to playlists from search results.
 */

#include "common/appstate.h"
#include "data/directorytree.h"

int displaySearch(int maxListSize, int indent, int *chosenRow, int startSearchIter);
int addToSearchText(const char *str);
int removeFromSearchText(void);
int getSearchResultsCount(void);
void freeSearchResults(void);
void fuzzySearch(FileSystemEntry *root, int threshold);
FileSystemEntry *getCurrentSearchEntry(void);
