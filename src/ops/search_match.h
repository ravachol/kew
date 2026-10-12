#ifndef SEARCH_MATCH_H
#define SEARCH_MATCH_H

#include <stdbool.h>

#include "data/directorytree.h"

#define SEARCH_NO_MATCH (-1)

/**
 * Scores a search query against an entry and, when enabled, its hierarchy.
 * Lower scores are better. SEARCH_NO_MATCH means that the query did not match.
 */
int search_match_entry(const char *query, const FileSystemEntry *entry, bool search_hierarchy);

#endif
