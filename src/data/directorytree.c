/**
 * @file directorytree.c
 * @brief Filesystem scanning and in-memory tree construction.
 *
 * Provides operations for recursively traversing directories and
 * constructing an in-memory linked tree representation of the music
 * library. Used by library and playlist modules to index songs efficiently.
 */

#include "directorytree.h"

#include "common/appstate.h"

#include "utils/file.h"
#include "utils/utils.h"

#include <dirent.h>
#include <glib.h>
#include <limits.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_STACK_SIZE 1000000

static int last_used_id = 0;

FileSystemEntry *create_entry(const char *name, int is_directory,
                             FileSystemEntry *parent)
{
        if (last_used_id == INT_MAX)
                return NULL;

        FileSystemEntry *newEntry = malloc(sizeof(FileSystemEntry));

        if (newEntry != NULL)
        {
                newEntry->name = strdup(name);
                if (newEntry->name == NULL)
                {
                        fprintf(stderr, "create_entry: name is null\n");
                        free(newEntry);
                        return NULL;
                }

                newEntry->is_directory = is_directory;
                newEntry->isEnqueued = 0;
                newEntry->parent = parent;
                newEntry->children = NULL;
                newEntry->next = NULL;
                newEntry->id = ++last_used_id;
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

void add_child(FileSystemEntry *parent, FileSystemEntry *child)
{
        if (parent != NULL)
        {
                child->next = parent->children;
                parent->children = child;
        }
}

#ifndef MAX_NAME
#define MAX_NAME 255
#endif

int is_valid_entry_name(const char *name)
{
        if (name == NULL)
                return 0;

        size_t len = strnlen(name, MAXPATHLEN + 1);
        if (len > MAX_NAME || len > MAXPATHLEN)
                return 0;

        if (len == 0)
                return 1;

        // Reject "." and ".." exactly
        if (len == 1 && name[0] == '.')
                return 0;
        if (len == 2 && name[0] == '.' && name[1] == '.')
                return 0;

        for (size_t i = 0; i < len; ++i)
        {
                unsigned char c = (unsigned char)name[i];

                // Reject path separators
                if (c == '/' || c == '\\')
                        return 0;

                // Reject ASCII control chars and DEL
                if (c <= 0x1F || c == 0x7F)
                        return 0;
        }

        return 1;
}

void set_full_path(FileSystemEntry *entry, const char *parentPath,
                 const char *entryName)
{
        if (entry == NULL || parentPath == NULL || entryName == NULL)
                return;

        if (!is_valid_entry_name(entryName))
        {
                char buf[257];
                snprintf(buf, sizeof(buf), "%s", entryName);
                buf[sizeof(buf) - 1] = '\0'; // ensure null-termination
                fprintf(stderr,
                        "Invalid entryName (possible path traversal): '%s'\n",
                        buf);

                return;
        }

        size_t parentLen = strnlen(parentPath, MAXPATHLEN + 1);
        size_t nameLen = strnlen(entryName, MAXPATHLEN + 1);

        if (parentLen > MAXPATHLEN || nameLen > MAXPATHLEN)
        {
                fprintf(
                    stderr,
                    "Parent or entry name too long or not null-terminated.\n");

                return;
        }

        if (parentLen > 0 && parentPath[parentLen - 1] == '/')
                parentLen--; // Normalize parent path (remove trailing slash)

        size_t needed = parentLen + 1 + nameLen + 1; // slash + null

        if (needed > MAXPATHLEN)
        {
                fprintf(stderr, "Path too long, rejecting.\n");

                return;
        }

        entry->fullPath = malloc(needed);

        if (entry->fullPath == NULL)
                return;

        snprintf(entry->fullPath, needed, "%.*s/%s", (int)parentLen, parentPath,
                 entryName);

        entry->fullPath[needed - 1] = '\0'; // Explicit null-termination

        // Post-check for directory traversal patterns
        if (strstr(entry->fullPath, "/../") != NULL ||
            strstr(entry->fullPath, "/..") ==
                entry->fullPath + strlen(entry->fullPath) - 3 ||
            strncmp(entry->fullPath, "../", 3) == 0)
        {
                fprintf(stderr,
                        "Path traversal attempt detected in fullPath: '%s'\n",
                        entry->fullPath);
                free(entry->fullPath);
                entry->fullPath = NULL;

                return;
        }
}

void free_tree(FileSystemEntry *root)
{
        if (root == NULL)
                return;

        size_t cap = 128, top = 0;
        FileSystemEntry **stack = malloc(cap * sizeof(*stack));

        if (!stack)
                return;

        stack[top++] = root;

        while (top > 0)
        {
                FileSystemEntry *node = stack[top - 1];

                if (node->children)
                {
                        if (top + 2 > cap)
                        {
                                if (cap > INT_MAX / 2 ||
                                    cap * 2 > MAX_STACK_SIZE)
                                {
                                        fprintf(stderr,
                                                "Stack capacity limit exceeded "
                                                "in free_tree\n");
                                        break;
                                }

                                size_t nc = cap * 2;
                                FileSystemEntry **tmp =
                                    realloc(stack, nc * sizeof(*stack));
                                if (!tmp)
                                        break;
                                stack = tmp;
                                cap = nc;
                        }

                        if (node->next)
                                stack[top++] = node->next;
                        stack[top++] = node->children;

                        node->children = NULL;
                        node->next = NULL;
                        continue;
                }

                top--;
                free(node->name);
                free(node->fullPath);
                free(node);
        }

        free(stack);
}

int natural_compare(const char *a, const char *b)
{
        while (*a && *b)
        {
                if (*a >= '0' && *a <= '9' && *b >= '0' && *b <= '9')
                {
                        // Parse number sequences
                        char *endA, *endB;
                        errno = 0;
                        unsigned long long numA = strtoull(a, &endA, 10);
                        int overflowA = (errno == ERANGE);

                        errno = 0;
                        unsigned long long numB = strtoull(b, &endB, 10);
                        int overflowB = (errno == ERANGE);

                        if (!overflowA && !overflowB)
                        {
                                if (numA < numB)
                                        return -1;
                                if (numA > numB)
                                        return 1;
                        }
                        else
                        {
                                // Fallback: compare digit length, then
                                // lexicographically
                                size_t lenA = endA - a;
                                size_t lenB = endB - b;
                                if (lenA < lenB)
                                        return -1;
                                if (lenA > lenB)
                                        return 1;
                                int cmp = strncmp(a, b, lenA);
                                if (cmp != 0)
                                        return cmp;
                        }

                        // Numbers equal, advance
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

        if (*a == 0 && *b == 0)
                return 0;
        if (*a == 0)
                return -1;
        return 1;
}

int compare_lib_entries(const struct dirent **a, const struct dirent **b)
{
        // All strings need to be uppercased or already uppercased characters
        // will come before all lower-case ones
        char *nameA = string_to_upper((*a)->d_name);
        char *nameB = string_to_upper((*b)->d_name);

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

        int result = natural_compare(nameA, nameB);

        free(nameA);
        free(nameB);

        return result;
}

int compare_lib_entries_reversed(const struct dirent **a, const struct dirent **b)
{
        int result = compare_lib_entries(a, b);

        return -result;
}

int compare_entry_natural(const void *a, const void *b)
{
        const FileSystemEntry *entryA = *(const FileSystemEntry **)a;
        const FileSystemEntry *entryB = *(const FileSystemEntry **)b;

        char *nameA = string_to_upper(entryA->name);
        char *nameB = string_to_upper(entryB->name);

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

        int result = natural_compare(nameA, nameB);

        free(nameA);
        free(nameB);
        return result;
}

int compare_entry_natural_reversed(const void *a, const void *b)
{
        return -compare_entry_natural(a, b);
}

#define MAX_RECURSION_DEPTH 1024

int remove_empty_directories(FileSystemEntry *node, int depth)
{
        if (node == NULL || depth > MAX_RECURSION_DEPTH)
                return 0;

        FileSystemEntry *currentChild = node->children;
        FileSystemEntry *prevChild = NULL;
        int numEntries = 0;

        while (currentChild != NULL)
        {
                if (currentChild->is_directory)
                {
                        numEntries +=
                            remove_empty_directories(currentChild, depth + 1);

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

int read_directory(const char *path, FileSystemEntry *parent)
{
        struct dirent **entries;
        int dirEntries =
            scandir(path, &entries, NULL, compare_lib_entries_reversed);

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

                if (entry->d_name[0] != '.' &&
                    strcmp(entry->d_name, ".") != 0 &&
                    strcmp(entry->d_name, "..") != 0)
                {
                        char childPath[MAXPATHLEN];
                        snprintf(childPath, sizeof(childPath), "%s/%s", path,
                                 entry->d_name);

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
                        extract_extension(entry->d_name, sizeof(exto) - 1, exto);

                        int isAudio = match_regex(&regex, exto);

                        if (isAudio == 0 || isDir)
                        {

                                FileSystemEntry *child =
                                    create_entry(entry->d_name, isDir, parent);

                                if (child == NULL)
                                        continue;

                                set_full_path(child, path, entry->d_name);

                                if (child->fullPath == NULL)
                                        continue;

                                add_child(parent, child);

                                if (isDir)
                                {
                                        numEntries++;
                                        numEntries +=
                                            read_directory(childPath, child);
                                }
                        }
                }

                free(entry);
        }

        free(entries);
        regfree(&regex);

        return numEntries;
}

void write_tree_to_file(FileSystemEntry *node, FILE *file, int parentId)
{
        if (node == NULL)
        {
                return;
        }

        fprintf(file, "%d\t%s\t%d\t%d\n", node->id, node->name,
                node->is_directory, parentId);

        FileSystemEntry *child = node->children;
        FileSystemEntry *tmp = NULL;
        while (child)
        {
                tmp = child->next;
                write_tree_to_file(child, file, node->id);
                child = tmp;
        }

        free(node->name);
        free(node->fullPath);
        free(node);
}

void free_and_write_tree(FileSystemEntry *root, const char *filename)
{
        FILE *file = fopen(filename, "w");
        if (!file)
        {
                perror("Failed to open file");
                return;
        }

        write_tree_to_file(root, file, -1);
        fclose(file);
}

FileSystemEntry *create_directory_tree(const char *startPath, int *numEntries)
{
        FileSystemEntry *root = create_entry("root", 1, NULL);

        set_library(root);

        set_full_path(root, "", "");

        *numEntries = read_directory(startPath, root);
        *numEntries -= remove_empty_directories(root, 0);

        last_used_id = 0;

        return root;
}

FileSystemEntry **resize_nodes_array(FileSystemEntry **nodes, int oldSize,
                                   int newSize)
{
        FileSystemEntry **newNodes =
            realloc(nodes, newSize * sizeof(FileSystemEntry *));
        if (newNodes)
        {
                for (int i = oldSize; i < newSize; i++)
                {
                        newNodes[i] = NULL;
                }
        }
        return newNodes;
}

int count_lines_and_max_id(const char *filename, int *maxId_out)
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

FileSystemEntry *reconstruct_tree_from_file(const char *filename,
                                         const char *startMusicPath,
                                         int *numDirectoryEntries)
{
        int maxId = -1;
        int nodeCount = count_lines_and_max_id(filename, &maxId);
        if (nodeCount <= 0 || maxId < 0)
                return NULL;

        FILE *file = fopen(filename, "r");
        if (!file)
                return NULL;

        // Allocate memory for maxid + 1 nodes
        FileSystemEntry **nodes =
            calloc((size_t)(maxId + 1), sizeof(FileSystemEntry *));
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
                if (sscanf(line, "%d\t%255[^\t]\t%d\t%d", &id, name, &isDir,
                           &parentId) != 4)
                        continue;
                FileSystemEntry *node = malloc(sizeof(FileSystemEntry));
                node->id = id;
                node->name = strdup(name);
                if (node->name == NULL)
                {
                        fprintf(stderr,
                                "reconstruct_tree_from_file:name is null\n");
                        free(node);
                        continue;
                }
                node->is_directory = isDir;
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
                                fprintf(stderr, "reconstructTreeFromFiley: "
                                                "fullPath is null\n");
                                free(node);
                                continue;
                        }
                        root = node;
                        set_library(root);
                }
        }
        fclose(file);

        free(nodes);
        return root;
}

// Calculates the Levenshtein distance.
// The Levenshtein distance between two strings is the minimum number of
// single-character edits (insertions, deletions, or substitutions) required to
// change one string into the other.
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

                        // Fill the current row with the minimum of deletion,
                        // insertion, or substitution
                        currRow[j] =
                            MIN(prevRow[j] + 1,              // Deletion
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

// Helper function to normalize and remove accents
char* normalize_string(const char *str)
{
        // First normalize to NFD (decomposed form) which separates base chars from accents
        char *normalized = g_utf8_normalize(str, -1, G_NORMALIZE_NFD);
        if (!normalized)
                return g_utf8_strdown(str, -1);

        // Then remove combining diacritical marks (accents)
        GString *result = g_string_new("");
        for (const char *p = normalized; *p; p = g_utf8_next_char(p)) {
                gunichar c = g_utf8_get_char(p);
                GUnicodeType type = g_unichar_type(c);
                // Skip combining marks (accents, diacritics)
                if (type != G_UNICODE_NON_SPACING_MARK &&
                    type != G_UNICODE_SPACING_MARK &&
                    type != G_UNICODE_ENCLOSING_MARK) {
                        g_string_append_unichar(result, g_unichar_tolower(c));
                }
        }

        g_free(normalized);
        return g_string_free(result, FALSE);
}

int calculate_search_distance(const char *needle, const char *haystack, int is_directory)
{
        // Convert to lowercase for case-insensitive matching
        char *needleLower = normalize_string(needle);
        char *haystackLower = normalize_string(haystack);

        int distance;

        // Check for exact match (distance 0)
        if (strcmp(haystackLower, needleLower) == 0) {
                distance = 0;
        }
        // Check for substring match (low distance based on extra chars)
        else if (strstr(haystackLower, needleLower) != NULL) {
                // Substring match: distance = extra characters
                int needleLen = g_utf8_strlen(needleLower, -1);
                int haystackLen = g_utf8_strlen(haystackLower, -1);
                distance = haystackLen - needleLen;
        }
        // Check if haystack starts with needle (prefix match)
        else if (g_str_has_prefix(haystackLower, needleLower)) {
                int needleLen = g_utf8_strlen(needleLower, -1);
                int haystackLen = g_utf8_strlen(haystackLower, -1);
                distance = haystackLen - needleLen;
        }
        // No substring match: use Levenshtein but add penalty
        else {
                int levenshtein = utf8_levenshteinDistance(needleLower, haystackLower);
                // Add large penalty to ensure substring matches rank higher
                int needleLen = g_utf8_strlen(needleLower, -1);
                distance = needleLen + levenshtein + 100;
        }

        // Add penalty for files (non-directories) to prioritize albums
        if (!is_directory) {
                distance += 25;
        }

        g_free(needleLower);
        g_free(haystackLower);

        return distance;
}

char *strip_file_extension(const char *filename)
{
        if (filename == NULL)
                return NULL;

        const char *dot = strrchr(filename, '.');

        // Don't treat a leading '.' as an extension (e.g. ".bashrc")
        if (dot == NULL || dot == filename)
                dot = filename + strlen(filename);

        size_t length = (size_t)(dot - filename);

        char *result = malloc(length + 1);
        if (!result)
        {
                perror("malloc");
                return NULL;
        }

        memcpy(result, filename, length);
        result[length] = '\0';

        return result;
}

// Traverses the tree and applies fuzzy search on each node
void fuzzy_search_recursive(FileSystemEntry *node, const char *searchTerm,
                          int threshold,
                          void (*callback)(FileSystemEntry *, int))
{
        if (node == NULL)
        {
                return;
        }

        // Convert search term, name, and fullPath to lowercase
        char *lowerSearchTerm = g_utf8_casefold(searchTerm, -1);
        char *lowerName = g_utf8_casefold(node->name, -1);

        int nameDistance =
            calculate_search_distance(lowerSearchTerm, lowerName, node->is_directory);

        // Partial matching with lowercase strings
        if (strstr(lowerName, lowerSearchTerm) != NULL)
        {
                callback(node, nameDistance);
        }
        else if (nameDistance <= threshold)
        {
                callback(node, nameDistance);
        }

        // Free the allocated memory for lowercase strings
        g_free(lowerSearchTerm);
        g_free(lowerName);

        fuzzy_search_recursive(node->children, searchTerm, threshold, callback);
        fuzzy_search_recursive(node->next, searchTerm, threshold, callback);
}

FileSystemEntry *find_corresponding_entry(FileSystemEntry *tmp,
                                        const char *fullPath)
{
        if (tmp == NULL)
                return NULL;
        if (strcmp(tmp->fullPath, fullPath) == 0)
                return tmp;

        FileSystemEntry *found =
            find_corresponding_entry(tmp->children, fullPath);
        if (found != NULL)
                return found;

        return find_corresponding_entry(tmp->next, fullPath);
}

void copy_is_enqueued(FileSystemEntry *library, FileSystemEntry *tmp)
{
        if (library == NULL)
                return;

        if (library->isEnqueued)
        {
                FileSystemEntry *tmpEntry =
                    find_corresponding_entry(tmp, library->fullPath);
                if (tmpEntry != NULL)
                {
                        tmpEntry->isEnqueued = library->isEnqueued;
                }
        }

        copy_is_enqueued(library->children, tmp);

        copy_is_enqueued(library->next, tmp);
}

int compare_folders_by_age_files_alphabetically(const void *a, const void *b)
{
        const FileSystemEntry *entryA = *(const FileSystemEntry **)a;
        const FileSystemEntry *entryB = *(const FileSystemEntry **)b;

        // Both are directories → sort by mtime descending
        if (entryA->is_directory && entryB->is_directory)
        {
                struct stat statA, statB;

                if (stat(entryA->fullPath, &statA) != 0 ||
                    stat(entryB->fullPath, &statB) != 0)
                        return 0;

                return (int)(statB.st_mtime - statA.st_mtime); // newer first
        }

        // Both are files → sort alphabetically
        if (!entryA->is_directory && !entryB->is_directory)
        {
                return strcasecmp(entryA->name, entryB->name);
        }

        // Put directories before files
        return entryB->is_directory - entryA->is_directory;
}

void sort_file_system_entry_children(FileSystemEntry *parent,
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

        FileSystemEntry **entryArray =
            malloc(count * sizeof(FileSystemEntry *));

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

void sort_file_system_tree(FileSystemEntry *root,
                        int (*comparator)(const void *, const void *))
{
        if (!root)
                return;

        sort_file_system_entry_children(root, comparator);

        FileSystemEntry *child = root->children;
        while (child)
        {
                if (child->is_directory)
                {
                        sort_file_system_tree(child, comparator);
                }
                child = child->next;
        }
}
