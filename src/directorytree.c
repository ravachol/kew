#include "directorytree.h"

FileSystemEntry *createEntry(const char *name, int isDirectory, FileSystemEntry *parent)
{
        FileSystemEntry *newEntry = (FileSystemEntry *)malloc(sizeof(FileSystemEntry));
        if (newEntry != NULL)
        {
                snprintf(newEntry->name, sizeof(newEntry->name), "%s", name);
                newEntry->isDirectory = isDirectory;
                newEntry->isEnqueued = 0;
                newEntry->parent = parent;
                newEntry->children = NULL;
                newEntry->next = NULL;
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
        snprintf(entry->fullPath, sizeof(entry->fullPath), "%s/%s", parentPath, entryName);
}

void displayTreeSimple(FileSystemEntry *root, int depth)
{
        for (int i = 0; i < depth; ++i)
        {
                printf("  "); // Add indentation for better visualization
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

                        // Remove the empty directory from the list
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

void freeTree(FileSystemEntry *root)
{
        if (root == NULL)
                return;

        FileSystemEntry *child = root->children;
        while (child != NULL)
        {
                FileSystemEntry *next = child->next;
                freeTree(child);
                child = next;
        }

        free(root);
}

char *stringToUpperWithoutSpaces(const char *str)
{
        if (str == NULL)
        {
                return NULL;
        }

        size_t len = strlen(str);
        char *result = (char *)malloc(len + 1); // +1 for the null terminator
        if (result == NULL)
        {
                // Handle memory allocation failure
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

        result[resultIndex] = '\0'; // Null-terminate the string

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
                                setFullPath(child, path, entry->d_name);                                
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

FileSystemEntry *createDirectoryTree(const char *startPath, int *numEntries)
{
        FileSystemEntry *root = createEntry("root", 1, NULL);

        *numEntries = readDirectory(startPath, root);
        *numEntries -= removeEmptyDirectories(root);

        return root;
}
