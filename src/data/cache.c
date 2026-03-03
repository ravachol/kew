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

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

Cache *create_cache()
{
        Cache *cache = malloc(sizeof(Cache));
        if (cache == NULL) {
                fprintf(stderr, "create_cache: malloc\n");
                return NULL;
        }
        cache->head = NULL;
        return cache;
}

void add_to_cache(Cache *cache, const char *file_path)
{
        if (cache == NULL) {
                fprintf(stderr, "Cache is null.\n");
                return;
        }

        if (file_path == NULL || *file_path == '\0') {
                fprintf(stderr, "Invalid file_path.\n");
                return;
        }

        if (strnlen(file_path, PATH_MAX + 1) >= PATH_MAX) {
                fprintf(stderr, "File path too long.\n");
                return;
        }

        CacheNode *new_node = malloc(sizeof(CacheNode));
        if (new_node == NULL) {
                fprintf(stderr, "add_to_cache: malloc\n");
                return;
        }

        new_node->file_path = strdup(file_path);
        if (new_node->file_path == NULL) {
                fprintf(stderr, "add_to_cache: strdup\n");
                free(new_node); // prevent memory leak
                return;
        }

        new_node->next = cache->head;
        cache->head = new_node;
}

void delete_cache(Cache *cache)
{
        if (cache == NULL) {
                fprintf(stderr, "delete_cache: Cache is null.\n");
                return;
        }

        CacheNode *current = cache->head;

        while (current != NULL) {
                CacheNode *tmp = current;
                current = current->next;
                free(tmp->file_path);
                free(tmp);
        }

        free(cache);
}

bool exists_in_cache(Cache *cache, char *file_path)
{
        if (file_path == NULL)
                return false;

        if (cache == NULL) {
                fprintf(stderr, "exists_in_cache: Cache is null.\n");
                return false;
        }

        CacheNode *current = cache->head;

        while (current != NULL) {
                if (strcmp(file_path, current->file_path) == 0) {
                        return true;
                }

                current = current->next;
        }

        return false;
}
