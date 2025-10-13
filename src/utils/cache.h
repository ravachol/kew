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

Cache *createCache(void);
void addToCache(Cache *cache, const char *filePath);
void deleteCache(Cache *cache);
bool existsInCache(Cache *cache, char *filePath);

#endif
