#include <ctype.h>
#include <dirent.h>
#include <glib.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "file.h"
#include "utils.h"
#include "directorytree.h"

/*

directorytree.c

 Related to library / directory structure.

*/

static int lastUsedId = 0;

typedef void (*TimeoutCallback)(void);

FileSystemEntry *createEntry(const char *name, int isDirectory, FileSystemEntry *parent)
{
        FileSystemEntry *newEntry = malloc(sizeof(FileSystemEntry));
        if (newEntry != NULL)
        {
                newEntry->name = strdup(name);
                if (newEntry->name == NULL)
                {
                        fprintf(stderr, "createEntry: name is null\n");
                        free(newEntry);
                        return NULL;
                }

                newEntry->isDirectory = isDirectory;
                newEntry->isEnqueued = 0;
                newEntry->parent = parent;
                newEntry->children = NULL;
                newEntry->next = NULL;
                newEntry->id = ++lastUsedId;
                if (parent != NULL)
                {
                        newEntry->parentId = parent->id;
                }
                else
                {
                        newEntry->parentId = -1;
                }
        }
        return newEntry;
}

void addChild(FileSystemEntry *parent, FileSystemEntry *child)
{
        if (parent != NULL)
        {
                child->next = parent->children;
                parent->children = child;
        }
}

int isValidEntryName(const char *name)
{
        if (name == NULL)
                return 0;

        // Reject path separators and traversal
        for (const char *p = name; *p; p++)
        {
                if (*p == '/' || *p == '\\')
                        return 0;
        }
        return 1;
}

void setFullPath(FileSystemEntry *entry, const char *parentPath, const char *entryName)
{
        if (entry == NULL || parentPath == NULL || entryName == NULL)
        {
                return;
        }

        if (!isValidEntryName(entryName))
        {
                // Limit printed length to avoid huge strings
                size_t maxLen = 256;
                char buf[257]; // +1 for null terminator
                strncpy(buf, entryName, maxLen);
                buf[maxLen] = '\0'; // ensure null-termination

                fprintf(stderr, "Invalid entryName (possible path traversal): '%s'\n", buf);
                return;
        }

        size_t parentLen = strnlen(parentPath, MAXPATHLEN);
        size_t nameLen = strnlen(entryName, MAXPATHLEN);

        // Check for overflow against MAXPATHLEN
        if (parentLen + nameLen + 2 > MAXPATHLEN)
        {

                return; // Path too long
        }
        size_t fullPathLength = parentLen + nameLen + 2; // +1 for '/' +1 for '\0'

        entry->fullPath = malloc(fullPathLength);
        if (entry->fullPath == NULL)
        {
                return;
        }
        snprintf(entry->fullPath, fullPathLength, "%s/%s", parentPath, entryName);
}

void freeTree(FileSystemEntry *root)
{
        if (root == NULL)
        {
                return;
        }

        FileSystemEntry *child = root->children;
        while (child != NULL)
        {
                FileSystemEntry *next = child->next;
                freeTree(child);
                child = next;
        }

        free(root->name);
        free(root->fullPath);

        free(root);
}

int naturalCompare(const char *a, const char *b)
{
        while (*a && *b)
        {
                if (isdigit((unsigned char)*a) && isdigit((unsigned char)*b))
                {
                        char *endA, *endB;
                        long numA = strtol(a, &endA, 10);
                        long numB = strtol(b, &endB, 10);

                        if (numA < numB)
                                return -1;
                        else if (numA > numB)
                                return 1;

                        // Numbers are equal, advance pointers
                        a = endA;
                        b = endB;
                }
                else
                {
                        if (*a != *b)
                                return (unsigned char)*a - (unsigned char)*b;

                        a++;
                        b++;
                }
        }

        // If all parts equal so far, shorter string comes first
        if (*a == 0 && *b == 0)
                return 0;
        else if (*a == 0)
                return -1;
        else
                return 1;
}

int compareLibEntries(const struct dirent **a, const struct dirent **b)
{
        // All strings need to be uppercased or already uppercased characters will come before all lower-case ones
        char *nameA = stringToUpper((*a)->d_name);
        char *nameB = stringToUpper((*b)->d_name);

        if (nameA[0] == '_' && nameB[0] != '_')
        {
                free(nameA);
                free(nameB);
                return 1;
        }
        else if (nameA[0] != '_' && nameB[0] == '_')
        {
                free(nameA);
                free(nameB);
                return -1;
        }

        int result = naturalCompare(nameA, nameB);

        free(nameA);
        free(nameB);

        return result;
}

int compareLibEntriesReversed(const struct dirent **a, const struct dirent **b)
{
        int result = compareLibEntries(a, b);

        return -result;
}

int compareEntryNatural(const void *a, const void *b)
{
        const FileSystemEntry *entryA = *(const FileSystemEntry **)a;
        const FileSystemEntry *entryB = *(const FileSystemEntry **)b;

        char *nameA = stringToUpper(entryA->name);
        char *nameB = stringToUpper(entryB->name);

        if (nameA[0] == '_' && nameB[0] != '_')
        {
                free(nameA);
                free(nameB);
                return 1;
        }
        else if (nameA[0] != '_' && nameB[0] == '_')
        {
                free(nameA);
                free(nameB);
                return -1;
        }

        int result = naturalCompare(nameA, nameB);

        free(nameA);
        free(nameB);
        return result;
}

int compareEntryNaturalReversed(const void *a, const void *b)
{
        return -compareEntryNatural(a, b);
}

#define MAX_RECURSION_DEPTH 1024

int removeEmptyDirectories(FileSystemEntry *node, int depth)
{
        if (node == NULL || depth > MAX_RECURSION_DEPTH)
                return 0;

        FileSystemEntry *currentChild = node->children;
        FileSystemEntry *prevChild = NULL;
        int numEntries = 0;

        while (currentChild != NULL)
        {
                if (currentChild->isDirectory)
                {
                        numEntries += removeEmptyDirectories(currentChild, depth+1);

                        if (currentChild->children == NULL)
                        {
                                if (prevChild == NULL)
                                {
                                        node->children = currentChild->next;
                                }
                                else
                                {
                                        prevChild->next = currentChild->next;
                                }

                                FileSystemEntry *toFree = currentChild;
                                currentChild = currentChild->next;

                                free(toFree->name);
                                free(toFree->fullPath);
                                free(toFree);
                                numEntries++;
                                continue;
                        }
                }

                prevChild = currentChild;
                currentChild = currentChild->next;
        }
        return numEntries;
}

int readDirectory(const char *path, FileSystemEntry *parent)
{
        struct dirent **entries;
        int dirEntries = scandir(path, &entries, NULL, compareLibEntriesReversed);

        if (dirEntries < 0)
        {
                return 0;
        }

        regex_t regex;
        regcomp(&regex, AUDIO_EXTENSIONS, REG_EXTENDED);

        int numEntries = 0;

        for (int i = 0; i < dirEntries; ++i)
        {
                struct dirent *entry = entries[i];

                if (entry == NULL)
                {
                        continue;
                }

                if (entry->d_name[0] != '.' && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
                {
                        char childPath[MAXPATHLEN];
                        snprintf(childPath, sizeof(childPath), "%s/%s", path, entry->d_name);

                        struct stat fileStats;
                        if (stat(childPath, &fileStats) == -1)
                        {
                                continue;
                        }

                        int isDir = true;

                        if (S_ISREG(fileStats.st_mode))
                        {
                                isDir = false;
                        }

                        char exto[100];
                        extractExtension(entry->d_name, sizeof(exto) - 1, exto);

                        int isAudio = match_regex(&regex, exto);

                        if (isAudio == 0 || isDir)
                        {

                                FileSystemEntry *child = createEntry(entry->d_name, isDir, parent);

                                if (child == NULL)
                                        continue;

                                setFullPath(child, path, entry->d_name);

                                if (child->fullPath == NULL)
                                        continue;

                                addChild(parent, child);

                                if (isDir)
                                {
                                        numEntries++;
                                        numEntries += readDirectory(childPath, child);
                                }
                        }
                }

                free(entry);
        }

        free(entries);
        regfree(&regex);

        return numEntries;
}

void writeTreeToFile(FileSystemEntry *node, FILE *file, int parentId)
{
        if (node == NULL)
        {
                return;
        }

        fprintf(file, "%d\t%s\t%d\t%d\n", node->id, node->name, node->isDirectory, parentId);

        FileSystemEntry *child = node->children;
        FileSystemEntry *tmp = NULL;
        while (child)
        {
                tmp = child->next;
                writeTreeToFile(child, file, node->id);
                child = tmp;
        }

        free(node->name);
        free(node->fullPath);
        free(node);
}

void freeAndWriteTree(FileSystemEntry *root, const char *filename)
{
        FILE *file = fopen(filename, "w");
        if (!file)
        {
                perror("Failed to open file");
                return;
        }

        writeTreeToFile(root, file, -1);
        fclose(file);
}

FileSystemEntry *createDirectoryTree(const char *startPath, int *numEntries)
{
        FileSystemEntry *root = createEntry("root", 1, NULL);

        setFullPath(root, "", "");

        *numEntries = readDirectory(startPath, root);
        *numEntries -= removeEmptyDirectories(root, 0);

        lastUsedId = 0;

        return root;
}

FileSystemEntry **resizeNodesArray(FileSystemEntry **nodes, int oldSize, int newSize)
{
        FileSystemEntry **newNodes = realloc(nodes, newSize * sizeof(FileSystemEntry *));
        if (newNodes)
        {
                for (int i = oldSize; i < newSize; i++)
                {
                        newNodes[i] = NULL;
                }
        }
        return newNodes;
}

int countLinesAndMaxId(const char *filename, int *maxId_out)
{
        FILE *file = fopen(filename, "r");
        if (!file)
                return -1;

        char line[1024];
        int lines = 0, maxId = -1;

        while (fgets(line, sizeof(line), file))
        {
                int id;
                if (sscanf(line, "%d", &id) == 1)
                {
                        if (id > maxId)
                                maxId = id;
                }
                lines++;
        }

        fclose(file);

        if (maxId_out)
                *maxId_out = maxId;

        return lines;
}

FileSystemEntry *reconstructTreeFromFile(const char *filename, const char *startMusicPath, int *numDirectoryEntries)
{
        int maxId = -1;
        int nodeCount = countLinesAndMaxId(filename, &maxId);
        if (nodeCount <= 0 || maxId < 0)
                return NULL;

        FILE *file = fopen(filename, "r");
        if (!file)
                return NULL;

        // Allocate memory for maxid + 1 nodes
        FileSystemEntry **nodes = calloc((size_t)(maxId + 1), sizeof(FileSystemEntry *));
        if (!nodes)
        {
                fclose(file);
                return NULL;
        }
        char line[1024];
        FileSystemEntry *root = NULL;
        if (numDirectoryEntries)
                *numDirectoryEntries = 0;

        while (fgets(line, sizeof(line), file))
        {
                int id, parentId, isDir;
                char name[256];
                if (sscanf(line, "%d\t%255[^\t]\t%d\t%d", &id, name, &isDir, &parentId) != 4)
                        continue;
                FileSystemEntry *node = malloc(sizeof(FileSystemEntry));
                node->id = id;
                node->name = strdup(name);
                if (node->name == NULL)
                {
                        fprintf(stderr, "reconstructTreeFromFile:name is null\n");
                        free(node);
                        continue;
                }
                node->isDirectory = isDir;
                node->isEnqueued = 0;
                node->parentId = parentId;
                node->parent = NULL;
                node->children = NULL;
                node->next = NULL;
                node->lastChild = NULL;
                nodes[id] = node;

                if (parentId >= 0 && parentId <= maxId && nodes[parentId])
                {
                        node->parent = nodes[parentId];
                        FileSystemEntry *parent = nodes[parentId];
                        if (!parent->children)
                        {
                                parent->children = parent->lastChild = node;
                        }
                        else
                        {
                                parent->lastChild->next = node;
                                parent->lastChild = node;
                        }
                        // fullPath = parent/fullName
                        size_t plen = strlen(parent->fullPath);
                        size_t nlen = strlen(name);
                        node->fullPath = malloc(plen + 1 + nlen + 1);
                        memcpy(node->fullPath, parent->fullPath, plen);
                        node->fullPath[plen] = '/';
                        memcpy(node->fullPath + plen + 1, name, nlen);
                        node->fullPath[plen + 1 + nlen] = 0;
                        if (isDir && numDirectoryEntries)
                                (*numDirectoryEntries)++;
                }
                else
                {
                        node->parent = NULL;
                        node->fullPath = strdup(startMusicPath);
                        if (node->fullPath == NULL)
                        {
                                fprintf(stderr, "reconstructTreeFromFiley: fullPath is null\n");
                                free(node);
                                continue;
                        }
                        root = node;
                }
        }
        fclose(file);

        free(nodes);
        return root;
}

#ifdef __GNUC__
#ifndef __APPLE__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#endif
// Calculates the Levenshtein distance.
// The Levenshtein distance between two strings is the minimum number of single-character edits
// (insertions, deletions, or substitutions) required to change one string into the other.
int utf8_levenshteinDistance(const char *s1, const char *s2)
{
        // Get the length of s1 and s2 in terms of characters, not bytes
        int len1 = g_utf8_strlen(s1, -1);
        int len2 = g_utf8_strlen(s2, -1);

        // Allocate a 2D matrix (only two rows at a time are needed)
        int *prevRow = malloc((len2 + 1) * sizeof(int));
        int *currRow = malloc((len2 + 1) * sizeof(int));

        if (prevRow == NULL)
        {
                if (currRow != NULL)
                        free(currRow);
                perror("malloc");
                return 0;
        }

        if (currRow == NULL)
        {
                free(prevRow);
                perror("malloc");
                return 0;
        }

        // Initialize the first row (for empty s1)
        for (int j = 0; j <= len2; j++)
        {
                prevRow[j] = j;
        }

        // Iterate over the characters of both strings
        const char *p1 = s1;
        for (int i = 1; i <= len1; i++, p1 = g_utf8_next_char(p1))
        {
                currRow[0] = i;
                const char *p2 = s2;
                for (int j = 1; j <= len2; j++, p2 = g_utf8_next_char(p2))
                {
                        // Compare Unicode characters using g_utf8_get_char
                        gunichar c1 = g_utf8_get_char(p1);
                        gunichar c2 = g_utf8_get_char(p2);

                        int cost = (c1 == c2) ? 0 : 1;

                        // Fill the current row with the minimum of deletion, insertion, or substitution
                        currRow[j] = MIN(prevRow[j] + 1,              // Deletion
                                         MIN(currRow[j - 1] + 1,      // Insertion
                                             prevRow[j - 1] + cost)); // Substitution
                }

                // Swap rows (current becomes previous for the next iteration)
                int *tmp = prevRow;
                prevRow = currRow;
                currRow = tmp;
        }

        // The last value in prevRow contains the Levenshtein distance
        int distance = prevRow[len2];

        // Free the allocated memory
        free(prevRow);
        free(currRow);

        return distance;
}

#ifdef __GNUC__
#ifndef __APPLE__
#pragma GCC diagnostic pop
#endif
#endif

char *stripFileExtension(const char *filename)
{
        const char *dot = strrchr(filename, '.'); // find last '.'
        size_t length = (dot != NULL) ? (size_t)(dot - filename) : strlen(filename);

        char *result = malloc(length + 1);
        if (!result)
        {
                perror("malloc");
                return NULL;
        }
        c_strcpy(result, filename, length);
        result[length] = '\0';

        return result;
}

// Traverses the tree and applies fuzzy search on each node
void fuzzySearchRecursive(FileSystemEntry *node, const char *searchTerm, int threshold, void (*callback)(FileSystemEntry *, int))
{
        if (node == NULL)
        {
                return;
        }

        // Convert search term, name, and fullPath to lowercase
        char *lowerSearchTerm = g_utf8_casefold(searchTerm, -1);
        char *lowerName = g_utf8_casefold(node->name, -1);

        char *strippedName = stripFileExtension(lowerName);

        int nameDistance = utf8_levenshteinDistance(strippedName, lowerSearchTerm);

        // Partial matching with lowercase strings
        if (strstr(strippedName, lowerSearchTerm) != NULL)
        {
                callback(node, 0);
        }
        else if (nameDistance <= threshold)
        {
                callback(node, nameDistance);
        }

        // Free the allocated memory for lowercase strings
        g_free(lowerSearchTerm);
        g_free(lowerName);
        free(strippedName);

        fuzzySearchRecursive(node->children, searchTerm, threshold, callback);
        fuzzySearchRecursive(node->next, searchTerm, threshold, callback);
}

FileSystemEntry *findCorrespondingEntry(FileSystemEntry *tmp, const char *fullPath)
{
        if (tmp == NULL)
                return NULL;
        if (strcmp(tmp->fullPath, fullPath) == 0)
                return tmp;

        FileSystemEntry *found = findCorrespondingEntry(tmp->children, fullPath);
        if (found != NULL)
                return found;

        return findCorrespondingEntry(tmp->next, fullPath);
}

void copyIsEnqueued(FileSystemEntry *library, FileSystemEntry *tmp)
{
        if (library == NULL)
                return;

        if (library->isEnqueued)
        {
                FileSystemEntry *tmpEntry = findCorrespondingEntry(tmp, library->fullPath);
                if (tmpEntry != NULL)
                {
                        tmpEntry->isEnqueued = library->isEnqueued;
                }
        }

        copyIsEnqueued(library->children, tmp);

        copyIsEnqueued(library->next, tmp);
}

int compareFoldersByAgeFilesAlphabetically(const void *a, const void *b)
{
        const FileSystemEntry *entryA = *(const FileSystemEntry **)a;
        const FileSystemEntry *entryB = *(const FileSystemEntry **)b;

        // Both are directories → sort by mtime descending
        if (entryA->isDirectory && entryB->isDirectory)
        {
                struct stat statA, statB;

                if (stat(entryA->fullPath, &statA) != 0 || stat(entryB->fullPath, &statB) != 0)
                        return 0;

                return (int)(statB.st_mtime - statA.st_mtime); // newer first
        }

        // Both are files → sort alphabetically
        if (!entryA->isDirectory && !entryB->isDirectory)
        {
                return strcasecmp(entryA->name, entryB->name);
        }

        // Put directories before files
        return entryB->isDirectory - entryA->isDirectory;
}

void sortFileSystemEntryChildren(FileSystemEntry *parent,
                                 int (*comparator)(const void *, const void *))
{
        int count = 0;
        FileSystemEntry *curr = parent->children;
        while (curr)
        {
                count++;
                curr = curr->next;
        }

        if (count < 2)
                return;

        FileSystemEntry **entryArray = malloc(count * sizeof(FileSystemEntry *));

        if (entryArray == NULL)
        {
                perror("malloc");
                return;
        }

        curr = parent->children;
        for (int i = 0; i < count; i++)
        {
                entryArray[i] = curr;
                curr = curr->next;
        }

        qsort(entryArray, count, sizeof(FileSystemEntry *), comparator);

        for (int i = 0; i < count - 1; i++)
        {
                entryArray[i]->next = entryArray[i + 1];
        }
        entryArray[count - 1]->next = NULL;
        parent->children = entryArray[0];

        free(entryArray);
}

void sortFileSystemTree(FileSystemEntry *root, int (*comparator)(const void *, const void *))
{
        if (!root)
                return;

        sortFileSystemEntryChildren(root, comparator);

        FileSystemEntry *child = root->children;
        while (child)
        {
                if (child->isDirectory)
                {
                        sortFileSystemTree(child, comparator);
                }
                child = child->next;
        }
}
