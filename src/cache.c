#define _XOPEN_SOURCE 700

#include "cache.h"
/*

cache.c

 Related to cache which contains paths to cached files.
 
*/

Cache *createCache()
{
        Cache *cache = (Cache *)malloc(sizeof(Cache));
        cache->head = NULL;
        return cache;
}

void addToCache(Cache *cache, const char *filePath)
{
        CacheNode *newNode = (CacheNode *)malloc(sizeof(CacheNode));
        newNode->filePath = strdup(filePath);
        newNode->next = cache->head;
        cache->head = newNode;
}

void deleteCache(Cache *cache)
{
        if (cache)
        {
                CacheNode *current = cache->head;
                while (current != NULL)
                {
                        CacheNode *temp = current;
                        current = current->next;
                        free(temp->filePath);
                        free(temp);
                }
                free(cache);
        }
}

bool existsInCache(Cache *cache, char *filePath)
{
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
