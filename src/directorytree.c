#include "directorytree.h"

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

        size_t fullPathLength = strlen(parentPath) + strlen(entryName) + 2; // +2 for '/' and '\0'

        entry->fullPath = (char *)malloc(fullPathLength);
        if (entry->fullPath == NULL)
        {
                return;
        }
        snprintf(entry->fullPath, fullPathLength, "%s/%s", parentPath, entryName);
}

void displayTreeSimple(FileSystemEntry *root, int depth)
{
        for (int i = 0; i < depth; ++i)
        {
                printf("  ");
        }

        printf("%s", root->name);
        if (root->isDirectory)
        {
                printf(" (Directory)\n");
                FileSystemEntry *child = root->children;
                while (child != NULL)
                {
                        displayTreeSimple(child, depth + 1);
                        child = child->next;
                }
        }
        else
        {
                printf(" (File)\n");
        }
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

char *stringToUpperWithoutSpaces(const char *str)
{
        if (str == NULL)
        {
                return NULL;
        }

        size_t len = strlen(str);
        char *result = (char *)malloc(len + 1);
        if (result == NULL)
        {
                return NULL;
        }

        size_t resultIndex = 0;
        for (size_t i = 0; i < len; ++i)
        {
                if (!isspace((unsigned char)str[i]))
                {
                        result[resultIndex++] = toupper((unsigned char)str[i]);
                }
        }

        result[resultIndex] = '\0';

        return result;
}

int compareLibEntries(const struct dirent **a, const struct dirent **b)
{
        char *nameA = stringToUpperWithoutSpaces((*a)->d_name);
        char *nameB = stringToUpperWithoutSpaces((*b)->d_name);

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

        int result = strcmp(nameB, nameA);
        free(nameA);
        free(nameB);

        return result;
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
        int dirEntries = scandir(path, &entries, NULL, compareLibEntries);
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

                        char exto[6];
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

int min(int a, int b, int c)
{
        if (a <= b && a <= c)
                return a;
        if (b <= a && b <= c)
                return b;
        return c;
}

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
// Calculates the Levenshtein distance.
// The Levenshtein distance between two strings is the minimum number of single-character edits
// (insertions, deletions, or substitutions) required to change one string into the other.
int levenshteinDistance(const char *s1, const char *s2)
{
        int len1 = strlen(s1);
        int len2 = strlen(s2);
        int **d = (int **)malloc((len1 + 1) * sizeof(int *));
        for (int i = 0; i <= len1; i++)
        {
                d[i] = (int *)malloc((len2 + 1) * sizeof(int));
                for (int j = 0; j <= len2; j++)
                {
                        d[i][j] = 0;
                }
        }

        for (int i = 0; i <= len1; i++)
        {
                d[i][0] = i;
        }
        for (int j = 0; j <= len2; j++)
        {
                d[0][j] = j;
        }

        for (int i = 1; i <= len1; i++)
        {
                for (int j = 1; j <= len2; j++)
                {
                        int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
                        d[i][j] = min(d[i - 1][j] + 1,         // deletion
                                      d[i][j - 1] + 1,         // insertion
                                      d[i - 1][j - 1] + cost); // substitution
                }
        }

        int distance = d[len1][len2];
        for (int i = 0; i <= len1; i++)
        {
                free(d[i]);
        }
        free(d);
        return distance;
}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

// Returns a new string that is lowercase of str
char *strLower(char *str)
{
        char *lowerStr = strdup(str);
        for (char *p = lowerStr; *p; ++p)
        {
                *p = tolower(*p);
        }
        return lowerStr;
}

// Traverses the tree and applies fuzzy search on each node
void fuzzySearchRecursive(FileSystemEntry *node, const char *searchTerm, int threshold, void (*callback)(FileSystemEntry *, int))
{
        if (node == NULL)
        {
                return;
        }

        // Convert search term, name, and fullPath to lowercase
        char *lowerSearchTerm = strLower((char *)searchTerm);
        char *lowerName = strLower(node->name);

        int nameDistance = levenshteinDistance(lowerName, lowerSearchTerm);

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
