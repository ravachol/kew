#define _XOPEN_SOURCE 700

/**
 * @file cache.c
 * @brief Disk cache management for metadata and library data.
 *
 * Handles caching of song metadata, album art, and library indexes
 * to improve startup and search performance.
 */

#include "cache.h"

#include "common/path_max.h"

#include "utils/k_log.h"
#include <stdlib.h>
#include <string.h>


Cache *create_cache()
{
        Cache *cache = malloc(sizeof(Cache));
        if (cache == NULL) {
                k_log("create_cache: malloc\n");
                return NULL;
        }
        cache->head = NULL;
        return cache;
}

void add_to_cache(Cache *cache, const char *file_path)
{
        if (cache == NULL) {
                k_log("Cache is null.\n");
                return;
        }

        if (file_path == NULL || *file_path == '\0') {
                k_log("Invalid file_path.\n");
                return;
        }

        if (strnlen(file_path, KEW_PATH_MAX + 1) >= KEW_PATH_MAX) {
                k_log("File path too long.\n");
                return;
        }

        CacheNode *new_node = malloc(sizeof(CacheNode));
        if (new_node == NULL) {
                k_log("add_to_cache: malloc\n");
                return;
        }

        new_node->file_path = strdup(file_path);
        if (new_node->file_path == NULL) {
                k_log("add_to_cache: strdup\n");
                free(new_node); // prevent memory leak
                return;
        }

        new_node->next = cache->head;
        cache->head = new_node;
}

void delete_cache(Cache *cache)
{
        if (cache == NULL) {
                k_log("delete_cache: Cache is null.\n");
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
                k_log("exists_in_cache: Cache is null.\n");
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
