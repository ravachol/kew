#ifndef CACHE_H
#define CACHE_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct CacheNode
{
        char *filePath;
        struct CacheNode *next;
} CacheNode;

typedef struct Cache
{
        CacheNode *head;
} Cache;

extern Cache *tempCache;

Cache *createCache();

void addToCache(Cache *cache, const char *filePath);

void deleteCache(Cache *cache);

void deleteCachedFiles(Cache *cache);

#endif