#include "directorytree.h"

/*

directorytree.c

 Related to library / directory structure.

*/

static int lastUsedId = 0;

typedef void (*TimeoutCallback)(void);

FileSystemEntry *createEntry(const char *name, int isDirectory, FileSystemEntry *parent)
{
        FileSystemEntry *newEntry = (FileSystemEntry *)malloc(sizeof(FileSystemEntry));
        if (newEntry != NULL)
        {
                newEntry->name = strdup(name);

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

void setFullPath(FileSystemEntry *entry, const char *parentPath, const char *entryName)
{
        if (entry == NULL || parentPath == NULL || entryName == NULL)
        {

                return;
        }

        size_t fullPathLength = strnlen(parentPath, MAXPATHLEN) + strnlen(entryName, MAXPATHLEN) + 2; // +2 for '/' and '\0'

        entry->fullPath = (char *)malloc(fullPathLength);
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

int removeEmptyDirectories(FileSystemEntry *node)
{
        if (node == NULL)
        {
                return 0;
        }

        FileSystemEntry *currentChild = node->children;
        FileSystemEntry *prevChild = NULL;
        int numEntries = 0;

        while (currentChild != NULL)
        {
                if (currentChild->isDirectory)
                {
                        numEntries += removeEmptyDirectories(currentChild);

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

        DIR *directory = opendir(path);
        if (directory == NULL)
        {
                perror("Error opening directory");
                return 0;
        }

        struct dirent **entries;
        int dirEntries = scandir(path, &entries, NULL, compareLibEntriesReversed);

        if (dirEntries < 0)
        {
                perror("Error scanning directory entries");
                closedir(directory);
                return 0;
        }

        regex_t regex;
        regcomp(&regex, AUDIO_EXTENSIONS, REG_EXTENDED);

        int numEntries = 0;

        for (int i = 0; i < dirEntries; ++i)
        {
                struct dirent *entry = entries[i];

                if (entry->d_name[0] != '.' && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
                {
                        char childPath[MAXPATHLEN];
                        snprintf(childPath, sizeof(childPath), "%s/%s", path, entry->d_name);

                        struct stat fileStats;
                        if (stat(childPath, &fileStats) == -1)
                        {
                                continue;
                        }

                        int isDirectory = true;

                        if (S_ISREG(fileStats.st_mode))
                        {
                                isDirectory = false;
                        }

                        char exto[100];
                        extractExtension(entry->d_name, sizeof(exto) - 1, exto);

                        int isAudio = match_regex(&regex, exto);

                        if (isAudio == 0 || isDirectory)
                        {
                                FileSystemEntry *child = createEntry(entry->d_name, isDirectory, parent);

                                if (entry != NULL)
                                {
                                        setFullPath(child, path, entry->d_name);
                                }

                                addChild(parent, child);

                                if (isDirectory)
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

        closedir(directory);

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
        *numEntries -= removeEmptyDirectories(root);

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

FileSystemEntry *reconstructTreeFromFile(const char *filename, const char *startMusicPath, int *numDirectoryEntries)
{
        FILE *file = fopen(filename, "r");
        if (!file)
        {
                return NULL;
        }

        char line[1024];
        int nodesCount = 0, nodesCapacity = 1000, oldCapacity = 0;
        FileSystemEntry **nodes = calloc(nodesCapacity, sizeof(FileSystemEntry *));
        if (!nodes)
        {
                fclose(file);
                return NULL;
        }

        FileSystemEntry *root = NULL;

        while (fgets(line, sizeof(line), file))
        {
                int id, parentId, isDirectory;
                char name[256];

                if (sscanf(line, "%d\t%255[^\t]\t%d\t%d", &id, name, &isDirectory, &parentId) == 4)
                {
                        if (id >= nodesCapacity)
                        {
                                oldCapacity = nodesCapacity;
                                nodesCapacity = id + 100;
                                FileSystemEntry **tempNodes = resizeNodesArray(nodes, oldCapacity, nodesCapacity);
                                if (!tempNodes)
                                {
                                        perror("Failed to resize nodes array");

                                        for (int i = 0; i < nodesCount; i++)
                                        {
                                                if (nodes[i])
                                                {
                                                        free(nodes[i]->name);
                                                        free(nodes[i]->fullPath);
                                                        free(nodes[i]);
                                                }
                                        }
                                        free(nodes);
                                        fclose(file);
                                        exit(EXIT_FAILURE);
                                }
                                nodes = tempNodes;
                        }

                        FileSystemEntry *node = malloc(sizeof(FileSystemEntry));
                        if (!node)
                        {
                                perror("Failed to allocate node");

                                fclose(file);
                                exit(EXIT_FAILURE);
                        }
                        node->id = id;
                        node->name = strdup(name);
                        node->isDirectory = isDirectory;
                        node->isEnqueued = 0;
                        node->children = node->next = node->parent = NULL;
                        nodes[id] = node;
                        nodesCount++;

                        if (parentId >= 0 && nodes[parentId])
                        {
                                node->parent = nodes[parentId];
                                if (nodes[parentId]->children)
                                {
                                        FileSystemEntry *child = nodes[parentId]->children;
                                        while (child->next)
                                        {
                                                child = child->next;
                                        }
                                        child->next = node;
                                }
                                else
                                {
                                        nodes[parentId]->children = node;
                                }

                                setFullPath(node, nodes[parentId]->fullPath, node->name);

                                if (isDirectory)
                                        *numDirectoryEntries = *numDirectoryEntries + 1;
                        }
                        else
                        {
                                root = node;
                                setFullPath(node, startMusicPath, "");
                        }
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
        int *prevRow = (int *)malloc((len2 + 1) * sizeof(int));
        int *currRow = (int *)malloc((len2 + 1) * sizeof(int));

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
                        currRow[j] = MIN(prevRow[j] + 1,              // deletion
                                         MIN(currRow[j - 1] + 1,      // insertion
                                             prevRow[j - 1] + cost)); // substitution
                }

                // Swap rows (current becomes previous for the next iteration)
                int *temp = prevRow;
                prevRow = currRow;
                currRow = temp;
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

// Traverses the tree and applies fuzzy search on each node
void fuzzySearchRecursive(FileSystemEntry *node, const char *searchTerm, int threshold, void (*callback)(FileSystemEntry *, int))
{
        if (node == NULL)
        {
                return;
        }

        // Convert search term, name, and fullPath to lowercase
        char *lowerSearchTerm = g_utf8_casefold((char *)searchTerm, -1);
        char *lowerName = g_utf8_casefold(node->name, -1);

        int nameDistance = utf8_levenshteinDistance(lowerName, lowerSearchTerm);

        // Partial matching with lowercase strings
        if (strstr(lowerName, lowerSearchTerm) != NULL)
        {
                callback(node, 0);
        }
        else if (nameDistance <= threshold)
        {
                callback(node, nameDistance);
        }

        // Free the allocated memory for lowercase strings
        free(lowerSearchTerm);
        free(lowerName);

        fuzzySearchRecursive(node->children, searchTerm, threshold, callback);
        fuzzySearchRecursive(node->next, searchTerm, threshold, callback);
}

FileSystemEntry *findCorrespondingEntry(FileSystemEntry *temp, const char *fullPath)
{
        if (temp == NULL)
                return NULL;
        if (strcmp(temp->fullPath, fullPath) == 0)
                return temp;

        FileSystemEntry *found = findCorrespondingEntry(temp->children, fullPath);
        if (found != NULL)
                return found;

        return findCorrespondingEntry(temp->next, fullPath);
}

void copyIsEnqueued(FileSystemEntry *library, FileSystemEntry *temp)
{
        if (library == NULL)
                return;

        if (library->isEnqueued)
        {
                FileSystemEntry *tempEntry = findCorrespondingEntry(temp, library->fullPath);
                if (tempEntry != NULL)
                {
                        tempEntry->isEnqueued = library->isEnqueued;
                }
        }

        copyIsEnqueued(library->children, temp);

        copyIsEnqueued(library->next, temp);
}
