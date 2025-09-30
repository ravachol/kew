#define _XOPEN_SOURCE 700

#include "cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*

cache.c

 Related to cache which contains paths to cached files.

*/

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

Cache *createCache()
{
        Cache *cache = malloc(sizeof(Cache));
        if (cache == NULL)
        {
                fprintf(stderr, "createCache: malloc\n");
                return NULL;
        }
        cache->head = NULL;
        return cache;
}

void addToCache(Cache *cache, const char *filePath)
{
        if (cache == NULL)
        {
                fprintf(stderr, "Cache is null.\n");
                return;
        }

        if (filePath == NULL || *filePath == '\0')
        {
                fprintf(stderr, "Invalid filePath.\n");
                return;
        }

        if (strlen(filePath) >= MAXPATHLEN)
        {
                fprintf(stderr, "File path too long.\n");
                return;
        }

        CacheNode *newNode = malloc(sizeof(CacheNode));
        if (newNode == NULL)
        {
                fprintf(stderr, "addToCache: malloc\n");
                return;
        }

        newNode->filePath = strdup(filePath);
        if (newNode->filePath == NULL)
        {
                fprintf(stderr, "addToCache: strdup\n");
                free(newNode); // prevent memory leak
                return;
        }

        newNode->next = cache->head;
        cache->head = newNode;
}

void deleteCache(Cache *cache)
{
        if (cache == NULL)
        {
                fprintf(stderr, "deleteCache: Cache is null.\n");
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
        if (filePath == NULL)
                return false;

        if (cache == NULL)
        {
                fprintf(stderr, "existsInCache: Cache is null.");
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
