#define _XOPEN_SOURCE 700

/**
 * @file cache.c
 * @brief Disk cache management for metadata and library data.
 *
 * Handles caching of song metadata, album art, and library indexes
 * to improve startup and search performance.
 */

#include "cache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

Cache *create_cache()
{
        Cache *cache = malloc(sizeof(Cache));
        if (cache == NULL)
        {
                fprintf(stderr, "create_cache: malloc\n");
                return NULL;
        }
        cache->head = NULL;
        return cache;
}

void add_to_cache(Cache *cache, const char *filePath)
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

        if (strnlen(filePath, MAXPATHLEN + 1) >= MAXPATHLEN)
        {
                fprintf(stderr, "File path too long.\n");
                return;
        }

        CacheNode *newNode = malloc(sizeof(CacheNode));
        if (newNode == NULL)
        {
                fprintf(stderr, "add_to_cache: malloc\n");
                return;
        }

        newNode->filePath = strdup(filePath);
        if (newNode->filePath == NULL)
        {
                fprintf(stderr, "add_to_cache: strdup\n");
                free(newNode); // prevent memory leak
                return;
        }

        newNode->next = cache->head;
        cache->head = newNode;
}

void delete_cache(Cache *cache)
{
        if (cache == NULL)
        {
                fprintf(stderr, "delete_cache: Cache is null.\n");
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

bool exists_in_cache(Cache *cache, char *filePath)
{
        if (filePath == NULL)
                return false;

        if (cache == NULL)
        {
                fprintf(stderr, "exists_in_cache: Cache is null.\n");
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
