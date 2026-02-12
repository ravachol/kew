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

typedef struct CacheNode {
        char *file_path;
        struct CacheNode *next;
} CacheNode;

typedef struct Cache {
        CacheNode *head;
} Cache;

/**
 * @brief Creates a new cache instance.
 *
 * This function allocates memory for a new cache and initializes its head to NULL.
 * The cache is returned for further use, or NULL is returned if memory allocation fails.
 *
 * @return A pointer to the created cache, or NULL if memory allocation fails.
 */
Cache *create_cache(void);


/**
 * @brief Adds a file path to the cache.
 *
 * This function adds the given file path to the cache, creating a new cache node and
 * linking it to the head of the cache. If the cache or file path is invalid, no action is taken.
 *
 * @param cache A pointer to the cache where the file path will be added.
 * @param file_path The file path to be added to the cache.
 */
void add_to_cache(Cache *cache, const char *file_path);


/**
 * @brief Deletes the cache and frees allocated memory.
 *
 * This function iterates through the cache and frees all memory used by the cache nodes and the cache itself.
 * If the cache is NULL, no action is performed.
 *
 * @param cache A pointer to the cache that should be deleted.
 */
void delete_cache(Cache *cache);


/**
 * @brief Checks if a file path exists in the cache.
 *
 * This function searches through the cache to determine if the given file path is already cached.
 * It returns true if the file path is found, or false if it is not.
 *
 * @param cache A pointer to the cache to search.
 * @param file_path The file path to search for in the cache.
 *
 * @return True if the file path is found in the cache, otherwise false.
 */
bool exists_in_cache(Cache *cache, char *file_path);

#endif
