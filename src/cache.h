#ifndef CACHE_H
#define CACHE_H

#include <stdbool.h>
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

Cache *createCache(void);

void addToCache(Cache *cache, const char *filePath);

void deleteCache(Cache *cache);

bool existsInCache(Cache *cache, char *filePath);

#endif
