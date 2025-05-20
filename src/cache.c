#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cache.h"
/*

cache.c

 Related to cache which contains paths to cached files.

*/

Cache *createCache()
{
        Cache *cache = malloc(sizeof(Cache));
        if (cache == NULL)
        {
                perror("malloc");
                return NULL;
        }
        cache->head = NULL;
        return cache;
}

void addToCache(Cache *cache, const char *filePath)
{
        if (cache == NULL)
        {
                fprintf(stderr, "Cache is null.");
                return;
        }
        CacheNode *newNode = malloc(sizeof(CacheNode));
        if (newNode == NULL)
        {
                perror("malloc");
                return;
        }
        newNode->filePath = strdup(filePath);
        newNode->next = cache->head;
        cache->head = newNode;
}

void deleteCache(Cache *cache)
{
        if (cache == NULL)
        {
                fprintf(stderr, "Cache is null.");
                return;
        }
        CacheNode *current = cache->head;
        while (current != NULL)
        {
                CacheNode *tmp = current;
                current = current->next;
                free(tmp->filePath);
                free(tmp);
        }
        free(cache);
}

bool existsInCache(Cache *cache, char *filePath)
{
        if (cache == NULL)
        {
                fprintf(stderr, "Cache is null.");
                return false;
        }
        CacheNode *current = cache->head;
        while (current != NULL)
        {
                if (strcmp(filePath, current->filePath) == 0)
                {
                        return true;
                }

                current = current->next;
        }
        return false;
}
