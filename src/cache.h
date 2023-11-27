#ifndef CACHE_H
#define CACHE_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

extern Cache *tempCache;

Cache *createCache(void);

void addToCache(Cache *cache, const char *filePath);

void deleteCache(Cache *cache);

void deleteCachedFiles(Cache *cache);

bool existsInCache(Cache *cache, char *filePath);

#endif
