/**
 * @file cache.h
 * @brief Disk cache management for metadata and library data.
 *
 * Handles caching of song metadata, album art, and library indexes
 * to improve startup and search performance.
 */

#ifndef CACHE_H
#define CACHE_H

#include <stdbool.h>

typedef struct CacheNode
{
        char *filePath;
        struct CacheNode *next;
} CacheNode;

typedef struct Cache
{
        CacheNode *head;
} Cache;

Cache *create_cache(void);
void add_to_cache(Cache *cache, const char *filePath);
void delete_cache(Cache *cache);
bool exists_in_cache(Cache *cache, char *filePath);

#endif
