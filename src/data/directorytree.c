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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define FSDB_MAGIC 0x46534442 // "FSDB"

static int last_used_id = 0;
static uint32_t DB_VERSION = 2;

// Header for the DB
typedef struct {
        uint32_t magic;
        uint32_t version;
        uint32_t entry_count;
        uint32_t string_table_size;
        uint32_t max_id;
        uint32_t root_full_path_offset;
        uint32_t root_full_path_length;
} FileSystemHeader;

// Per-entry on disk (fixed-size fields + offsets into string table)
typedef struct {
        int32_t id;
        int32_t parent_id;
        int32_t is_directory;
        int32_t is_enqueued;
        uint32_t name_offset;
} FileSystemEntryDisk;

typedef struct {
        FileSystemEntry **data;
        size_t size;
        size_t capacity;
} EntryArray;

static void entry_array_init(EntryArray *arr)
{
        arr->size = 0;
        arr->capacity = 1024;
        arr->data = malloc(sizeof(FileSystemEntry *) * arr->capacity);
}

static void entry_array_push(EntryArray *arr, FileSystemEntry *entry)
{
        if (arr->size >= arr->capacity) {
                arr->capacity *= 2;
                arr->data = realloc(arr->data, sizeof(FileSystemEntry *) * arr->capacity);
        }
        arr->data[arr->size++] = entry;
}

static void collect_entries(FileSystemEntry *node, EntryArray *arr)
{
        if (!node)
                return;

        entry_array_push(arr, node);

        for (FileSystemEntry *child = node->children; child; child = child->next)
                collect_entries(child, arr);
}

int write_tree_to_binary(FileSystemEntry *root, const char *filename)
{
        if (!root || !filename)
                return -1;

        // Flatten tree
        EntryArray arr;
        entry_array_init(&arr);
        collect_entries(root, &arr);
        if (arr.size == 0) {
                free(arr.data);
                return -1; // nothing to write
        }

        // Compute max ID (for loader safety)
        uint32_t max_id = 0;
        for (size_t i = 0; i < arr.size; i++) {
                if ((uint32_t)arr.data[i]->id > max_id)
                        max_id = arr.data[i]->id;
        }

        // Build string table (names only)
        size_t total_name_size = 0;
        for (size_t i = 0; i < arr.size; i++) {
                if (!arr.data[i]->name)
                        continue;
                total_name_size += strlen(arr.data[i]->name) + 1;
        }

        char *string_table = malloc(total_name_size);
        if (!string_table) {
                free(arr.data);
                return -1;
        }

        FileSystemEntryDisk *disk_entries = malloc(sizeof(FileSystemEntryDisk) * arr.size);
        if (!disk_entries) {
                free(arr.data);
                free(string_table);
                return -1;
        }

        // Populate disk entries and string table
        size_t offset = 0;
        for (size_t i = 0; i < arr.size; i++) {
                FileSystemEntry *n = arr.data[i];
                FileSystemEntryDisk *d = &disk_entries[i];

                d->id = n->id;
                d->parent_id = n->parent_id;
                d->is_directory = n->is_directory;
                d->is_enqueued = n->is_enqueued;

                if (n->name) {
                        d->name_offset = (uint32_t)offset;
                        strcpy(&string_table[offset], n->name);
                        offset += strlen(n->name) + 1;
                } else {
                        d->name_offset = UINT32_MAX; // fallback if name is NULL
                }
        }

        FileSystemHeader header = {
            .magic = FSDB_MAGIC,
            .version = DB_VERSION,
            .entry_count = (uint32_t)arr.size,
            .max_id = max_id,
            .string_table_size = (uint32_t)total_name_size,
            .root_full_path_offset = 0,
            .root_full_path_length = 0};

        header.root_full_path_offset = offset;

        // Store the root path
        size_t root_len = strlen(root->full_path) + 1;
        header.root_full_path_length = (uint32_t)root_len;

        // Increase the string table in memory
        string_table = realloc(string_table, offset + root_len);
        memcpy(string_table + offset, root->full_path, root_len);

        // Update final string table size
        header.string_table_size = (uint32_t)(offset + root_len);

        FILE *f = fopen(filename, "wb");
        if (!f) {
                free(arr.data);
                free(disk_entries);
                free(string_table);
                return -1;
        }

        if (fwrite(&header, sizeof(header), 1, f) != 1 ||
            fwrite(disk_entries, sizeof(FileSystemEntryDisk), arr.size, f) != arr.size ||
            fwrite(string_table, 1, header.string_table_size, f) != header.string_table_size) {

                fclose(f);
                free(arr.data);
                free(disk_entries);
                free(string_table);
                return -1;
        }

        fclose(f);
        free(arr.data);
        free(disk_entries);
        free(string_table);

        return 0;
}

FileSystemEntry *create_entry(const char *name, int is_directory,
                              FileSystemEntry *parent)
{
        if (last_used_id == INT_MAX)
                return NULL;

        FileSystemEntry *new_entry = malloc(sizeof(FileSystemEntry));

        if (new_entry != NULL) {
                new_entry->name = strdup(name);

                if (new_entry->name == NULL) {
                        fprintf(stderr, "create_entry: name is null\n");
                        free(new_entry);
                        return NULL;
                }

                new_entry->is_directory = is_directory;
                new_entry->is_enqueued = 0;
                new_entry->parent = parent;
                new_entry->children = NULL;
                new_entry->next = NULL;
                new_entry->id = ++last_used_id;

                if (parent != NULL) {
                        new_entry->parent_id = parent->id;
                } else {
                        new_entry->parent_id = -1;
                }
        }
        return new_entry;
}

void add_child(FileSystemEntry *parent, FileSystemEntry *child)
{
        if (parent != NULL) {
                child->next = parent->children;
                parent->children = child;
        }
}

int is_valid_entry_name(const char *name)
{
        if (name == NULL)
                return 0;

        size_t len = strnlen(name, PATH_MAX + 1);
        if (len > NAME_MAX || len > PATH_MAX)
                return 0;

        if (len == 0)
                return 1;

        // Reject "." and ".." exactly
        if (len == 1 && name[0] == '.')
                return 0;
        if (len == 2 && name[0] == '.' && name[1] == '.')
                return 0;

        for (size_t i = 0; i < len; ++i) {
                unsigned char c = (unsigned char)name[i];

                // Reject path separator
                if (c == '/')
                        return 0;

                // Reject ASCII control chars and DEL
                if (c <= 0x1F || c == 0x7F)
                        return 0;
        }

        return 1;
}

void set_full_path(FileSystemEntry *entry, const char *parent_path,
                   const char *entry_name)
{
        if (entry == NULL || parent_path == NULL || entry_name == NULL)
                return;

        if (!is_valid_entry_name(entry_name)) {
                return;
        }

        size_t parentLen = strnlen(parent_path, PATH_MAX + 1);
        size_t nameLen = strnlen(entry_name, PATH_MAX + 1);

        if (parentLen > PATH_MAX || nameLen > PATH_MAX) {
                fprintf(
                    stderr,
                    "Parent or entry name too long or not null-terminated.\n");

                return;
        }

        if (parentLen > 0 && parent_path[parentLen - 1] == '/')
                parentLen--; // Normalize parent path (remove trailing slash)

        size_t needed = parentLen + 1 + nameLen + 1; // slash + null

        if (needed > PATH_MAX) {
                fprintf(stderr, "Path too long, rejecting.\n");

                return;
        }

        entry->full_path = malloc(needed);

        if (entry->full_path == NULL)
                return;

        if (nameLen == 0)
                snprintf(entry->full_path, needed, "%s", parent_path);
        else

                snprintf(entry->full_path, needed, "%.*s/%s", (int)parentLen, parent_path,
                         entry_name);

        entry->full_path[needed - 1] = '\0'; // Explicit null-termination

        // Post-check for directory traversal patterns
        if (strstr(entry->full_path, "/../") != NULL ||
            strstr(entry->full_path, "/..") ==
                entry->full_path + strlen(entry->full_path) - 3 ||
            strncmp(entry->full_path, "../", 3) == 0) {
                fprintf(stderr,
                        "Path traversal attempt detected in full_path: '%s'\n",
                        entry->full_path);
                free(entry->full_path);
                entry->full_path = NULL;

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

        while (top > 0) {
                FileSystemEntry *node = stack[--top];

                // Push siblings first (to handle next pointers)
                if (node->next) {
                        if (top + 1 >= cap) {
                                cap *= 2;
                                FileSystemEntry **tmp = realloc(stack, cap * sizeof(*stack));
                                if (!tmp)
                                        break;
                                stack = tmp;
                        }
                        stack[top++] = node->next;
                }

                // Push children (so they get freed before the parent)
                if (node->children) {
                        if (top + 1 >= cap) {
                                cap *= 2;
                                FileSystemEntry **tmp = realloc(stack, cap * sizeof(*stack));
                                if (!tmp)
                                        break;
                                stack = tmp;
                        }
                        stack[top++] = node->children;
                }

                // Now free this node itself
                free(node->name);
                free(node->full_path);
                free(node);
        }

        free(stack);
}

int natural_compare(const char *a, const char *b)
{
        while (*a && *b) {
                if (*a >= '0' && *a <= '9' && *b >= '0' && *b <= '9') {
                        // Parse number sequences
                        char *end_a, *endB;
                        errno = 0;
                        unsigned long long num_a = strtoull(a, &end_a, 10);
                        int overflow_a = (errno == ERANGE);

                        errno = 0;
                        unsigned long long num_b = strtoull(b, &endB, 10);
                        int overflow_b = (errno == ERANGE);

                        if (!overflow_a && !overflow_b) {
                                if (num_a < num_b)
                                        return -1;
                                if (num_a > num_b)
                                        return 1;
                        } else {
                                // Fallback: compare digit length, then
                                // lexicographically
                                size_t lenA = end_a - a;
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
                        a = end_a;
                        b = endB;
                } else {
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
        char *name_a = string_to_upper((*a)->d_name);
        char *name_b = string_to_upper((*b)->d_name);

        if (name_a[0] == '_' && name_b[0] != '_') {
                free(name_a);
                free(name_b);
                return 1;
        } else if (name_a[0] != '_' && name_b[0] == '_') {
                free(name_a);
                free(name_b);
                return -1;
        }

        int result = natural_compare(name_a, name_b);

        free(name_a);
        free(name_b);

        return result;
}

int compare_lib_entries_reversed(const struct dirent **a, const struct dirent **b)
{
        int result = compare_lib_entries(a, b);

        return -result;
}

int compare_entry_natural(const void *a, const void *b)
{
        const FileSystemEntry *entry_a = *(const FileSystemEntry **)a;
        const FileSystemEntry *entry_b = *(const FileSystemEntry **)b;

        char *name_a = string_to_upper(entry_a->name);
        char *name_b = string_to_upper(entry_b->name);

        if (name_a[0] == '_' && name_b[0] != '_') {
                free(name_a);
                free(name_b);
                return 1;
        } else if (name_a[0] != '_' && name_b[0] == '_') {
                free(name_a);
                free(name_b);
                return -1;
        }

        int result = natural_compare(name_a, name_b);

        free(name_a);
        free(name_b);
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

        FileSystemEntry *current_child = node->children;
        FileSystemEntry *prev_child = NULL;
        int num_entries = 0;

        while (current_child != NULL) {
                if (current_child->is_directory) {
                        num_entries +=
                            remove_empty_directories(current_child, depth + 1);

                        if (current_child->children == NULL) {
                                if (prev_child == NULL) {
                                        node->children = current_child->next;
                                } else {
                                        prev_child->next = current_child->next;
                                }

                                FileSystemEntry *to_free = current_child;
                                current_child = current_child->next;

                                free(to_free->name);
                                free(to_free->full_path);
                                free(to_free);
                                num_entries++;
                                continue;
                        }
                }

                prev_child = current_child;
                current_child = current_child->next;
        }
        return num_entries;
}

int read_directory(const char *path, FileSystemEntry *parent)
{
        struct dirent **entries;
        int dir_entries =
            scandir(path, &entries, NULL, compare_lib_entries_reversed);

        if (dir_entries < 0) {
                return 0;
        }

        regex_t regex;
        regcomp(&regex, AUDIO_EXTENSIONS, REG_EXTENDED | REG_ICASE);

        int num_entries = 0;

        for (int i = 0; i < dir_entries; ++i) {
                struct dirent *entry = entries[i];

                if (entry == NULL) {
                        continue;
                }

                if (entry->d_name[0] != '.' &&
                    strcmp(entry->d_name, ".") != 0 &&
                    strcmp(entry->d_name, "..") != 0) {
                        char child_path[PATH_MAX];
                        snprintf(child_path, sizeof(child_path), "%s/%s", path,
                                 entry->d_name);

                        struct stat file_stats;
                        if (stat(child_path, &file_stats) == -1) {
                                continue;
                        }

                        int is_dir = true;

                        if (S_ISREG(file_stats.st_mode)) {
                                is_dir = false;
                        }

                        char exto[100];
                        extract_extension(entry->d_name, sizeof(exto) - 1, exto);

                        int is_audio = match_regex(&regex, exto);

                        if (is_audio == 0 || is_dir) {

                                FileSystemEntry *child =
                                    create_entry(entry->d_name, is_dir, parent);

                                if (child == NULL)
                                        continue;

                                set_full_path(child, path, entry->d_name);

                                if (child->full_path == NULL) {
                                        free(child);
                                        continue;
                                }

                                add_child(parent, child);

                                if (is_dir) {
                                        num_entries++;
                                        num_entries +=
                                            read_directory(child_path, child);
                                }
                        }
                }

                free(entry);
        }

        free(entries);
        regfree(&regex);

        return num_entries;
}

FileSystemEntry *create_directory_tree(const char *start_path, int *num_entries)
{
        FileSystemEntry *root = create_entry("root", 1, NULL);

        set_library(root);

        set_full_path(root, start_path, "");

        *num_entries = read_directory(start_path, root);
        *num_entries -= remove_empty_directories(root, 0);

        last_used_id = 0;

        return root;
}

FileSystemEntry **resize_nodes_array(FileSystemEntry **nodes, int old_size,
                                     int new_size)
{
        FileSystemEntry **new_nodes =
            realloc(nodes, new_size * sizeof(FileSystemEntry *));
        if (new_nodes) {
                for (int i = old_size; i < new_size; i++) {
                        new_nodes[i] = NULL;
                }
        }
        return new_nodes;
}

int count_lines_and_max_id(const char *filename, int *max_id_out)
{
        FILE *file = fopen(filename, "r");
        if (!file)
                return -1;

        char line[1024];
        int lines = 0, max_id = -1;

        while (fgets(line, sizeof(line), file)) {
                int id;
                if (sscanf(line, "%d", &id) == 1) {
                        if (id > max_id)
                                max_id = id;
                }
                lines++;
        }

        fclose(file);

        if (max_id_out)
                *max_id_out = max_id;

        return lines;
}

FileSystemEntry *read_tree_from_binary(
    const char *filename,
    const char *start_music_path,
    int *num_directory_entries, bool set_enqueued_status)
{
        if (!filename || !start_music_path)
                return NULL;
        if (num_directory_entries)
                *num_directory_entries = 0;

        FILE *f = fopen(filename, "rb");
        if (!f)
                return NULL;

        FileSystemHeader header;
        if (fread(&header, sizeof(header), 1, f) != 1 || header.magic != FSDB_MAGIC || header.version != DB_VERSION) {
                fclose(f);
                return NULL;
        }

        // Read disk entries
        FileSystemEntryDisk *disk_entries = malloc(sizeof(FileSystemEntryDisk) * header.entry_count);
        if (!disk_entries) {
                fclose(f);
                return NULL;
        }

        if (fread(disk_entries, sizeof(FileSystemEntryDisk), header.entry_count, f) != header.entry_count) {
                free(disk_entries);
                fclose(f);
                return NULL;
        }

        // Read string table
        char *string_table = malloc(header.string_table_size);
        if (!string_table) {
                free(disk_entries);
                fclose(f);
                return NULL;
        }

        if (fread(string_table, 1, header.string_table_size, f) != header.string_table_size) {
                free(disk_entries);
                free(string_table);
                fclose(f);
                return NULL;
        }

        fclose(f);

        // Determine max ID to safely allocate array
        int max_id = -1;
        for (uint32_t i = 0; i < header.entry_count; i++)
                if (disk_entries[i].id > max_id)
                        max_id = disk_entries[i].id;

        FileSystemEntry **nodes = calloc((size_t)max_id + 1, sizeof(FileSystemEntry *));
        if (!nodes) {
                free(disk_entries);
                free(string_table);
                return NULL;
        }

        const char *stored_root_path = string_table + header.root_full_path_offset;

        FileSystemEntry *root = NULL;

        for (uint32_t i = 0; i < header.entry_count; i++) {
                FileSystemEntryDisk *d = &disk_entries[i];

                if (d->id < 0 || d->id > max_id)
                        continue;

                FileSystemEntry *n = malloc(sizeof(FileSystemEntry));
                if (!n)
                        continue;

                n->id = d->id;
                n->parent_id = d->parent_id;
                n->is_directory = d->is_directory;
                if (set_enqueued_status)
                        n->is_enqueued = d->is_enqueued;
                else
                        n->is_enqueued = 0;
                if (d->name_offset == UINT32_MAX)
                        n->name = strdup(""); // empty name
                else
                        n->name = strdup(string_table + d->name_offset);
                n->parent = n->children = n->next = n->lastChild = NULL;
                n->full_path = NULL;

                nodes[n->id] = n;

                // Link to parent if exists
                if (n->parent_id >= 0 && n->parent_id <= max_id && nodes[n->parent_id]) {
                        FileSystemEntry *p = nodes[n->parent_id];
                        n->parent = p;
                        if (!p->children)
                                p->children = p->lastChild = n;
                        else {
                                p->lastChild->next = n;
                                p->lastChild = n;
                        }

                        // full_path = parent/full_name
                        size_t plen = strlen(p->full_path);
                        size_t nlen = strlen(n->name);
                        n->full_path = malloc(plen + 1 + nlen + 1);
                        if (n->full_path) {
                                memcpy(n->full_path, p->full_path, plen);
                                n->full_path[plen] = '/';
                                memcpy(n->full_path + plen + 1, n->name, nlen);
                                n->full_path[plen + 1 + nlen] = '\0';
                        } else {
                                n->full_path = strdup(n->name);
                        }

                        if (n->is_directory && num_directory_entries)
                                (*num_directory_entries)++;

                } else {
                        // Root node
                        root = n;
                        n->parent = NULL;
                        n->full_path = strdup(stored_root_path);
                }
        }

        free(nodes);
        free(disk_entries);
        free(string_table);

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
        int *prev_row = malloc((len2 + 1) * sizeof(int));
        int *curr_row = malloc((len2 + 1) * sizeof(int));

        if (prev_row == NULL) {
                if (curr_row != NULL)
                        free(curr_row);
                perror("malloc");
                return 0;
        }

        if (curr_row == NULL) {
                free(prev_row);
                perror("malloc");
                return 0;
        }

        // Initialize the first row (for empty s1)
        for (int j = 0; j <= len2; j++) {
                prev_row[j] = j;
        }

        // Iterate over the characters of both strings
        const char *p1 = s1;
        for (int i = 1; i <= len1; i++, p1 = g_utf8_next_char(p1)) {
                curr_row[0] = i;
                const char *p2 = s2;
                for (int j = 1; j <= len2; j++, p2 = g_utf8_next_char(p2)) {
                        // Compare Unicode characters using g_utf8_get_char
                        gunichar c1 = g_utf8_get_char(p1);
                        gunichar c2 = g_utf8_get_char(p2);

                        int cost = (c1 == c2) ? 0 : 1;

                        // Fill the current row with the minimum of deletion,
                        // insertion, or substitution
                        curr_row[j] =
                            MIN(prev_row[j] + 1,              // Deletion
                                MIN(curr_row[j - 1] + 1,      // Insertion
                                    prev_row[j - 1] + cost)); // Substitution
                }

                // Swap rows (current becomes previous for the next iteration)
                int *tmp = prev_row;
                prev_row = curr_row;
                curr_row = tmp;
        }

        // The last value in prev_row contains the Levenshtein distance
        int distance = prev_row[len2];

        // Free the allocated memory
        free(prev_row);
        free(curr_row);

        return distance;
}

// Helper function to normalize and remove accents
char *normalize_string(const char *str)
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
        char *needle_lower = normalize_string(needle);
        char *haystack_lower = normalize_string(haystack);

        int distance;

        // Check for exact match (distance 0)
        if (strcmp(haystack_lower, needle_lower) == 0) {
                distance = 0;
        }
        // Check for substring match (low distance based on extra chars)
        else if (strstr(haystack_lower, needle_lower) != NULL) {
                // Substring match: distance = extra characters
                int needle_len = g_utf8_strlen(needle_lower, -1);
                int haystack_len = g_utf8_strlen(haystack_lower, -1);
                distance = haystack_len - needle_len;
        }
        // Check if haystack starts with needle (prefix match)
        else if (g_str_has_prefix(haystack_lower, needle_lower)) {
                int needle_len = g_utf8_strlen(needle_lower, -1);
                int haystack_len = g_utf8_strlen(haystack_lower, -1);
                distance = haystack_len - needle_len;
        }
        // No substring match: use Levenshtein but add penalty
        else {
                int levenshtein = utf8_levenshteinDistance(needle_lower, haystack_lower);
                // Add large penalty to ensure substring matches rank higher
                int needle_len = g_utf8_strlen(needle_lower, -1);
                distance = needle_len + levenshtein + 100;
        }

        // Add penalty for files (non-directories) to prioritize albums
        if (!is_directory) {
                distance += 25;
        }

        g_free(needle_lower);
        g_free(haystack_lower);

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
        if (!result) {
                perror("malloc");
                return NULL;
        }

        memcpy(result, filename, length);
        result[length] = '\0';

        return result;
}

// Traverses the tree and applies fuzzy search on each node
void fuzzy_search_recursive(FileSystemEntry *node, const char *search_term,
                            int threshold,
                            void (*callback)(FileSystemEntry *, int))
{
        if (node == NULL) {
                return;
        }

        // Convert search term, name, and full_path to lowercase
        char *lower_search_term = g_utf8_casefold(search_term, -1);
        char *lower_name = g_utf8_casefold(node->name, -1);

        int name_distance =
            calculate_search_distance(lower_search_term, lower_name, node->is_directory);

        // Partial matching with lowercase strings
        if (strstr(lower_name, lower_search_term) != NULL) {
                callback(node, name_distance);
        } else if (name_distance <= threshold) {
                callback(node, name_distance);
        }

        // Free the allocated memory for lowercase strings
        g_free(lower_search_term);
        g_free(lower_name);

        fuzzy_search_recursive(node->children, search_term, threshold, callback);
        fuzzy_search_recursive(node->next, search_term, threshold, callback);
}

FileSystemEntry *find_corresponding_entry(FileSystemEntry *tmp,
                                          const char *full_path)
{
        if (tmp == NULL)
                return NULL;
        if (strcmp(tmp->full_path, full_path) == 0)
                return tmp;

        FileSystemEntry *found =
            find_corresponding_entry(tmp->children, full_path);
        if (found != NULL)
                return found;

        return find_corresponding_entry(tmp->next, full_path);
}

void copy_is_enqueued(FileSystemEntry *library, FileSystemEntry *tmp)
{
        if (library == NULL)
                return;

        if (library->is_enqueued) {
                FileSystemEntry *tmp_entry =
                    find_corresponding_entry(tmp, library->full_path);
                if (tmp_entry != NULL) {
                        tmp_entry->is_enqueued = library->is_enqueued;
                }
        }

        copy_is_enqueued(library->children, tmp);

        copy_is_enqueued(library->next, tmp);
}

int compare_folders_by_age_files_alphabetically(const void *a, const void *b)
{
        const FileSystemEntry *entry_a = *(const FileSystemEntry **)a;
        const FileSystemEntry *entry_b = *(const FileSystemEntry **)b;

        // Both are directories → sort by mtime descending
        if (entry_a->is_directory && entry_b->is_directory) {
                struct stat stat_a, statB;

                if (stat(entry_a->full_path, &stat_a) != 0 ||
                    stat(entry_b->full_path, &statB) != 0)
                        return 0;

                return (int)(statB.st_mtime - stat_a.st_mtime); // newer first
        }

        // Both are files → sort alphabetically
        if (!entry_a->is_directory && !entry_b->is_directory) {
                return strcasecmp(entry_a->name, entry_b->name);
        }

        // Put directories before files
        return entry_b->is_directory - entry_a->is_directory;
}

void sort_file_system_entry_children(FileSystemEntry *parent,
                                     int (*comparator)(const void *, const void *))
{
        int count = 0;
        FileSystemEntry *curr = parent->children;
        while (curr) {
                count++;
                curr = curr->next;
        }

        if (count < 2)
                return;

        FileSystemEntry **entry_array =
            malloc(count * sizeof(FileSystemEntry *));

        if (entry_array == NULL) {
                perror("malloc");
                return;
        }

        curr = parent->children;
        for (int i = 0; i < count; i++) {
                entry_array[i] = curr;
                curr = curr->next;
        }

        qsort(entry_array, count, sizeof(FileSystemEntry *), comparator);

        for (int i = 0; i < count - 1; i++) {
                entry_array[i]->next = entry_array[i + 1];
        }
        entry_array[count - 1]->next = NULL;
        parent->children = entry_array[0];

        free(entry_array);
}

void sort_file_system_tree(FileSystemEntry *root,
                           int (*comparator)(const void *, const void *))
{
        if (!root)
                return;

        sort_file_system_entry_children(root, comparator);

        FileSystemEntry *child = root->children;
        while (child) {
                if (child->is_directory) {
                        sort_file_system_tree(child, comparator);
                }
                child = child->next;
        }
}
