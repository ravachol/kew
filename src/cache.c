#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cache.h"

Cache *tempCache = NULL;

Cache* createCache() {
    Cache *cache = malloc(sizeof(Cache));
    cache->head = NULL;
    return cache;
}

void addToCache(Cache *cache, const char *filePath) {
    CacheNode *newNode = malloc(sizeof(CacheNode));
    newNode->filePath = strdup(filePath);
    newNode->next = cache->head;
    cache->head = newNode;
}

void deleteCache(Cache *cache) {
    CacheNode *current = cache->head;
    while (current != NULL) {
        CacheNode *temp = current;
        current = current->next;
        free(temp->filePath);
        free(temp);
    }
    free(cache);
}

void deleteCachedFiles(Cache *cache) {
    CacheNode *current = cache->head;
    while (current != NULL) {
        remove(current->filePath);
        current = current->next;
    }
}