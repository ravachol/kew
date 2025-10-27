#define _XOPEN_SOURCE 700
#define __USE_XOPEN_EXTENDED 1

/**
 * @file playlist.c
 * @brief Playlist data structures and operations.
 *
 * Defines the core playlist data types and provides functions for managing
 * track lists, iterating songs, and maintaining play order. Used by both
 * playback and UI modules to coordinate current and upcoming tracks.
 */

#include "playlist.h"

#include "common/appstate.h"

#include "utils/file.h"
#include "utils/utils.h"

#include <glib.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SEARCH_SIZE 256

const char PLAYLIST_EXTENSIONS[] = "(m3u8?)$";
const char favorites_playlist_name[] = "kew favorites.m3u";
const char last_used_playlist_name[] = "lastPlaylist.m3u";

static char search[MAX_SEARCH_SIZE];
static char playlist_name[MAX_SEARCH_SIZE];
static bool shuffle = false;
static int num_dirs = 0;
static Node *current_song = NULL;
static int node_id_counter = 0;

void clear_current_song(void) { current_song = NULL; }

void set_current_song(Node *node) { current_song = node; }

Node *get_current_song(void) { return current_song; }

Node *get_list_next(Node *node) { return (node == NULL) ? NULL : node->next; }

Node *get_list_prev(Node *node) { return (node == NULL) ? NULL : node->prev; }

int add_to_list(PlayList *list, Node *newNode)
{
        if (newNode == NULL)
                return 0;

        if (list == NULL || list->count >= MAX_FILES)
                return -1;

        list->count++;

        if (list->head == NULL)
        {
                newNode->prev = NULL;
                list->head = newNode;
                list->tail = newNode;
        }
        else
        {
                newNode->prev = list->tail;
                list->tail->next = newNode;
                list->tail = newNode;
        }

        return 0;
}

void move_up_list(PlayList *list, Node *node)
{
        if (node == list->head || node == NULL || node->prev == NULL)
                return;

        Node *prevNode = node->prev;
        Node *nextNode = node->next;

        if (prevNode->prev)
                prevNode->prev->next = node;
        else
                list->head = node;

        node->prev = prevNode->prev;
        node->next = prevNode;

        prevNode->prev = node;
        prevNode->next = nextNode;

        if (nextNode)
                nextNode->prev = prevNode;
        else
                list->tail = prevNode;
}

void move_down_list(PlayList *list, Node *node)
{
        if (node == list->tail || node == NULL || node->next == NULL)
                return;

        Node *nextNode = node->next;
        Node *prevNode = node->prev;
        Node *nextNextNode = nextNode->next;

        if (prevNode)
                prevNode->next = nextNode;
        else
                list->head = nextNode;

        nextNode->prev = prevNode;
        nextNode->next = node;

        node->prev = nextNode;
        node->next = nextNextNode;

        if (nextNextNode)
                nextNextNode->prev = node;
        else
                list->tail = node;
}

Node *delete_from_list(PlayList *list, Node *node)
{
        if (list == NULL || node == NULL || list->head == NULL)
                return NULL;

        Node *nextNode = node->next;

        // Adjust head and tail
        if (list->head == node)
                list->head = nextNode;
        if (list->tail == node)
                list->tail = node->prev;

        // Fix neighbors
        if (node->prev != NULL)
                node->prev->next = nextNode;
        if (nextNode != NULL)
                nextNode->prev = node->prev;

        // Free song file path string if allocated
        if (node->song.filePath != NULL)
        {
                free(node->song.filePath);
                node->song.filePath = NULL;
        }

        free(node);
        node = NULL;

        list->count--;

        return nextNode;
}

void delete_playlist(PlayList *list)
{
        if (list == NULL)
                return;

        pthread_mutex_lock(&list->mutex);

        Node *current = list->head;
        while (current != NULL)
        {
                Node *next = current->next;
                free(current->song.filePath);
                free(current);
                current = next;
        }

        list->head = NULL;
        list->tail = NULL;
        list->count = 0;

        pthread_mutex_unlock(&list->mutex);
        pthread_mutex_destroy(&list->mutex);
}

void shuffle_playlist(PlayList *playlist)
{
        if (playlist == NULL || playlist->count <= 1)
        {
                return; // No need to shuffle
        }

        // Check for overflow before malloc
        if ((size_t)playlist->count > SIZE_MAX / sizeof(Node *))
        {
                printf(_("Playlist too large to allocate.\n"));
                // atexit() will free up resources properly
                exit(1);
        }

        // Convert the linked list to an array
        Node **nodes = (Node **)malloc(playlist->count * sizeof(Node *));
        if (nodes == NULL)
        {
                printf(_("Memory allocation error.\n"));
                // atexit() will free up resources properly
                exit(1);
        }

        Node *current = playlist->head;
        int i = 0;
        while (current != NULL)
        {
                nodes[i++] = current;
                current = current->next;
        }

        // Shuffle the array using Fisher-Yates algorithm
        for (int j = playlist->count - 1; j >= 1; --j)
        {
                int k = rand() % (j + 1);
                Node *tmp = nodes[j];
                nodes[j] = nodes[k];
                nodes[k] = tmp;
        }

        playlist->head = nodes[0];
        playlist->tail = nodes[playlist->count - 1];
        for (int j = 0; j < playlist->count; ++j)
        {
                nodes[j]->next =
                    (j < playlist->count - 1) ? nodes[j + 1] : NULL;
                nodes[j]->prev = (j > 0) ? nodes[j - 1] : NULL;
        }
        free(nodes);
}

void insert_as_first(Node *current_song, PlayList *playlist)
{
        if (current_song == NULL || playlist == NULL)
        {
                return;
        }

        if (playlist->head == NULL)
        {
                current_song->next = NULL;
                current_song->prev = NULL;
                playlist->head = current_song;
                playlist->tail = current_song;
        }
        else
        {
                if (current_song != playlist->head)
                {
                        if (current_song->next != NULL)
                        {
                                current_song->next->prev = current_song->prev;
                        }
                        else
                        {
                                playlist->tail = current_song->prev;
                        }

                        if (current_song->prev != NULL)
                        {
                                current_song->prev->next = current_song->next;
                        }

                        // Add the current_song as the new head
                        current_song->next = playlist->head;
                        current_song->prev = NULL;
                        playlist->head->prev = current_song;
                        playlist->head = current_song;
                }
        }
}

void shuffle_playlist_starting_from_song(PlayList *playlist, Node *song)
{
        shuffle_playlist(playlist);
        if (song != NULL && playlist->count > 1)
        {
                insert_as_first(song, playlist);
        }
}

void create_node(Node **node, const char *directoryPath, int id)
{
        SongInfo song;
        song.filePath = strdup(directoryPath);
        song.duration = 0.0;

        *node = (Node *)malloc(sizeof(Node));
        if (*node == NULL)
        {
                printf(_("Failed to allocate memory."));
                free(song.filePath);
                exit(0);
                return;
        }

        (*node)->song = song;
        (*node)->next = NULL;
        (*node)->prev = NULL;
        (*node)->id = id;
}

void destroy_node(Node *node)
{
        if (node == NULL)
                return;

        if (node->song.filePath != NULL)
                free(node->song.filePath);

        free(node);
}

void exit_if_overflow(int counter)
{
        if (counter == INT_MAX)
        {
                fprintf(stderr,
                        "Error: Node ID overflow. Max node limit reached.\n");
                exit(1);
        }
}

void build_playlist_recursive(const char *directoryPath,
                            const char *allowedExtensions, PlayList *playlist)
{
        char expandedPath[MAXPATHLEN - NAME_MAX - 1];
        expand_path(directoryPath, expandedPath);

        int res = is_directory(expandedPath);
        if (res != 1 && res != -1 && directoryPath != NULL)
        {
                Node *node = NULL;

                exit_if_overflow(node_id_counter);
                create_node(&node, expandedPath, node_id_counter++);
                if (add_to_list(playlist, node) == -1)
                        destroy_node(node);

                return;
        }

        DIR *dir = opendir(expandedPath);
        if (dir == NULL)
        {
                printf(_("Failed to open directory: %s\n"), expandedPath);
                return;
        }

        regex_t regex;
        int ret = regcomp(&regex, allowedExtensions, REG_EXTENDED);
        if (ret != 0)
        {
                printf(_("Failed to compile regular expression\n"));
                closedir(dir);
                return;
        }

        char exto[100];
        struct dirent **entries;
        int numEntries =
            scandir(expandedPath, &entries, NULL, compare_lib_entries);

        if (numEntries < 0)
        {
                printf(_("Failed to scan directory: %s\n"), expandedPath);
                return;
        }

        for (int i = 0; i < numEntries && playlist->count < MAX_FILES; i++)
        {
                struct dirent *entry = entries[i];

                if (entry->d_name[0] == '.' ||
                    strcmp(entry->d_name, ".") == 0 ||
                    strcmp(entry->d_name, "..") == 0)
                {
                        continue;
                }

                char filePath[FILENAME_MAX];
                snprintf(filePath, sizeof(filePath), "%s/%s", expandedPath,
                         entry->d_name);

                size_t pathLen = strnlen(expandedPath, sizeof(expandedPath));
                size_t nameLen = strnlen(entry->d_name, NAME_MAX);

                if (pathLen + 1 + nameLen >= FILENAME_MAX)
                {
                        fprintf(stderr, "Path too long: %s/%s\n", expandedPath, entry->d_name);
                        return; // or skip this entry
                }

                snprintf(filePath, sizeof(filePath), "%s/%s", expandedPath, entry->d_name);

                if (is_directory(filePath) == 1)
                {
                        int songCount = playlist->count;
                        build_playlist_recursive(filePath, allowedExtensions,
                                               playlist);
                        if (playlist->count > songCount)
                                num_dirs++;
                }
                else
                {
                        extract_extension(entry->d_name, sizeof(exto) - 1, exto);
                        if (match_regex(&regex, exto) == 0)
                        {
                                snprintf(filePath, sizeof(filePath), "%s/%s",
                                         directoryPath, entry->d_name);

                                Node *node = NULL;

                                exit_if_overflow(node_id_counter);
                                create_node(&node, filePath, node_id_counter++);
                                if (add_to_list(playlist, node) == -1)
                                        destroy_node(node);
                        }
                }
        }

        for (int i = 0; i < numEntries; i++)
        {
                free(entries[i]);
        }
        free(entries);

        closedir(dir);
        regfree(&regex);
}

int join_playlist(PlayList *dest, PlayList *src)
{
        if (src->count == 0)
        {
                return 0;
        }

        if (dest->count == 0)
        {
                dest->head = src->head;
                dest->tail = src->tail;
        }
        else
        {
                dest->tail->next = src->head;
                src->head->prev = dest->tail;
                dest->tail = src->tail;
        }
        dest->count += src->count;

        src->head = NULL;
        src->tail = NULL;
        src->count = 0;

        return 1;
}

void make_playlist_name(const char *search, int maxSize)
{
        playlist_name[0] = '\0';

        snprintf(playlist_name, maxSize, "%s", search);

        snprintf(playlist_name + strnlen(playlist_name, maxSize),
                 maxSize - strnlen(playlist_name, maxSize), ".m3u");

        for (int i = 0; playlist_name[i] != '\0'; i++)
        {
                if (playlist_name[i] == ':')
                {
                        playlist_name[i] = '-';
                }
        }
}

void read_m3_u_file(const char *filepath, PlayList *playlist,
                 FileSystemEntry *library)
{
        GError *error = NULL;
        gchar *contents;
        gchar **lines;

        char filename[MAXPATHLEN];
        expand_path(filepath, filename);

        if (!g_file_get_contents(filename, &contents, NULL, &error))
        {
                g_clear_error(&error);
                return;
        }

        gchar *directory = g_path_get_dirname(filename);

        lines = g_strsplit(contents, "\n", -1);

        for (gint i = 0; lines[i] != NULL; i++)
        {
                gchar *line = lines[i];

                line = g_strdelimit(line, "\r", '\0');
                gchar *trimmed_line = g_strstrip(line);

                if (trimmed_line[0] != '#' && trimmed_line[0] != '\0')
                {
                        gchar *songPath;

                        if (g_path_is_absolute(trimmed_line))
                        {
                                songPath = g_strdup(trimmed_line);
                        }
                        else
                        {
                                songPath = g_build_filename(directory,
                                                            trimmed_line, NULL);
                        }

                        if (songPath == NULL)
                                continue;

                        if (exists_file(songPath) < 0)
                                continue;

                        // Don't add songs that are already enqueued
                        if (library != NULL)
                        {
                                FileSystemEntry *entry =
                                    find_corresponding_entry(library, songPath);

                                if (entry != NULL && entry->isEnqueued)
                                {
                                        g_free(songPath);
                                        continue;
                                }
                        }

                        Node *newNode = NULL;
                        create_node(&newNode, songPath, node_id_counter++);

                        if (playlist->head == NULL)
                        {
                                playlist->head = newNode;
                                playlist->tail = newNode;
                        }
                        else
                        {
                                playlist->tail->next = newNode;
                                newNode->prev = playlist->tail;
                                playlist->tail = newNode;
                        }

                        playlist->count++;

                        g_free(songPath);
                }
        }

        g_free(directory);
        g_strfreev(lines);
        g_free(contents);
}

int make_playlist(PlayList **playlist, int argc, char *argv[], bool exactSearch, const char *path)
{
        const char *delimiter = ":";
        const char *allowedExtensions = MUSIC_FILE_EXTENSIONS;

        enum SearchType searchType = SearchAny;
        int searchTypeIndex = 1;
        PlayList partialPlaylist = {NULL, NULL, 0, PTHREAD_MUTEX_INITIALIZER};

        char expandedPath[MAXPATHLEN];
        expand_path(path, expandedPath);

        if (strcmp(argv[1], "all") == 0)
        {
                searchType = ReturnAllSongs;
                shuffle = true;
        }

        if (argc > 1)
        {
                if (strcmp(argv[1], "list") == 0 && argc > 2)
                {
                        allowedExtensions = PLAYLIST_EXTENSIONS;
                        searchType = SearchPlayList;
                }

                if (strcmp(argv[1], "random") == 0 ||
                    strcmp(argv[1], "rand") == 0 ||
                    strcmp(argv[1], "shuffle") == 0)
                {
                        int count = 0;
                        while (argv[count] != NULL)
                        {
                                count++;
                        }
                        if (count > 2)
                        {
                                searchTypeIndex = 2;
                                shuffle = true;
                        }
                }

                if (strcmp(argv[searchTypeIndex], "dir") == 0)
                        searchType = DirOnly;
                else if (strcmp(argv[searchTypeIndex], "song") == 0)
                        searchType = FileOnly;
        }

        int start = searchTypeIndex + 1;

        if (searchType == FileOnly || searchType == DirOnly ||
            searchType == SearchPlayList)
                start = searchTypeIndex + 2;

        search[0] = '\0';

        for (int i = start - 1; i < argc; i++)
        {
                size_t len = strnlen(search, MAX_SEARCH_SIZE);
                snprintf(search + len, MAX_SEARCH_SIZE - len, " %s", argv[i]);
        }

        make_playlist_name(search, MAX_SEARCH_SIZE);

        if (strstr(search, delimiter))
        {
                shuffle = true;
        }

        if (searchType == ReturnAllSongs)
        {
                pthread_mutex_lock(&((*playlist)->mutex));

                build_playlist_recursive(expandedPath, allowedExtensions, *playlist);

                pthread_mutex_unlock(&((*playlist)->mutex));
        }
        else
        {
                char *token = strtok(search, delimiter);

                while (token != NULL)
                {
                        char buf[MAXPATHLEN] = {0};
                        if (strncmp(token, "song", 4) == 0)
                        {
                                memmove(token, token + 4,
                                        strnlen(token + 4, MAXPATHLEN) + 1);
                                searchType = FileOnly;
                        }
                        trim(token, MAXPATHLEN);
                        char *searching = g_utf8_casefold(token, -1);

                        if (walker(expandedPath, searching, buf, allowedExtensions,
                                   searchType, exactSearch, 0) == 0)
                        {
                                if (strcmp(argv[1], "list") == 0)
                                {
                                        read_m3_u_file(buf, *playlist, NULL);
                                }
                                else
                                {
                                        pthread_mutex_lock(&((*playlist)->mutex));

                                        build_playlist_recursive(
                                            buf, allowedExtensions,
                                            &partialPlaylist);
                                        join_playlist(*playlist,
                                                     &partialPlaylist);

                                        pthread_mutex_unlock(
                                            &((*playlist)->mutex));
                                }
                        }
                        free(searching);
                        token = strtok(NULL, delimiter);
                }
        }
        if (num_dirs > 1)
                shuffle = true;
        if (shuffle)
                shuffle_playlist(*playlist);

        if ((*playlist)->head == NULL)
                printf(_("Music not found\n"));

        return 0;
}

void generate_m3_u_filename(const char *basePath, const char *filePath,
                         char *m3uFilename, size_t size)
{

        const char *baseName = strrchr(filePath, '/');
        if (baseName == NULL)
        {
                baseName = filePath; // No '/' found, use the entire filename
        }
        else
        {
                baseName++; // Skip the '/' character
        }

        const char *dot = strrchr(baseName, '.');
        if (dot == NULL)
        {
                // No '.' found, copy the base name and append ".m3u"
                if (basePath[strnlen(basePath, MAXPATHLEN) - 1] == '/')
                {
                        snprintf(m3uFilename, size, "%s%s.m3u", basePath,
                                 baseName);
                }
                else
                {
                        snprintf(m3uFilename, size, "%s/%s.m3u", basePath,
                                 baseName);
                }
        }
        else
        {
                // Copy the base name up to the dot and append ".m3u"
                size_t baseNameLen = dot - baseName;
                if (basePath[strnlen(basePath, MAXPATHLEN) - 1] == '/')
                {
                        snprintf(m3uFilename, size, "%s%.*s.m3u", basePath,
                                 (int)baseNameLen, baseName);
                }
                else
                {
                        snprintf(m3uFilename, size, "%s/%.*s.m3u", basePath,
                                 (int)baseNameLen, baseName);
                }
        }
}

void write_m3u_file(const char *filename, const PlayList *playlist)
{
        FILE *file = fopen(filename, "w");
        if (file == NULL)
        {
                return;
        }

        Node *currentNode = playlist->head;
        while (currentNode != NULL)
        {
                fprintf(file, "%s\n", currentNode->song.filePath);
                currentNode = currentNode->next;
        }
        fclose(file);
}

void load_playlist(const char *directory, const char *playlist_name,
                  PlayList **playlist)
{
        char playlistPath[MAXPATHLEN];

        if (!directory || !playlist_name || !playlist)
                return;

        size_t len = strnlen(directory, MAXPATHLEN);
        if (len == 0)
                return;

        snprintf(playlistPath, sizeof(playlistPath), "%s%s%s",
                 directory,
                 (directory[len - 1] == '/' ? "" : "/"),
                 playlist_name);

        if (*playlist == NULL)
        {
                create_playlist(playlist);
        }

        if (*playlist)
                read_m3_u_file(playlistPath, *playlist, NULL);
}

void load_favorites_playlist(const char *directory, PlayList **favorites_playlist)
{
        char expandedPath[MAXPATHLEN];
        expand_path(directory, expandedPath);
        load_playlist(directory, favorites_playlist_name, favorites_playlist);
        set_favorites_playlist(*favorites_playlist);
}

void load_last_used_playlist(PlayList *playlist, PlayList **unshuffled_playlist)
{
        char *configdir = get_config_path();

        load_playlist(configdir, last_used_playlist_name, &playlist);
        set_playlist(playlist);

        if (*unshuffled_playlist == NULL)
        {
                *unshuffled_playlist = deep_copy_play_list(playlist);
        }

        if (configdir)
                free(configdir);
}

void save_named_playlist(const char *directory, const char *playlist_name,
                       const PlayList *playlist)
{
        if (directory == NULL)
        {
                return;
        }

        char playlistPath[MAXPATHLEN];

        int length =
            snprintf(playlistPath, sizeof(playlistPath), "%s", directory);

        if (length <= 0 || length >= (int)sizeof(playlistPath) ||
            playlistPath[0] == '\0')
        {
                return;
        }

        if (playlistPath[length - 1] != '/')
        {
                snprintf(playlistPath + length, sizeof(playlistPath) - length,
                         "/%s", playlist_name);
        }
        else
        {
                snprintf(playlistPath + length, sizeof(playlistPath) - length,
                         "%s", playlist_name);
        }

        if (playlist != NULL)
        {
                write_m3u_file(playlistPath, playlist);
        }
}

void save_favorites_playlist(const char *directory, PlayList *favorites_playlist)
{
        char expandedPath[MAXPATHLEN];
        expand_path(directory, expandedPath);

        if (favorites_playlist != NULL && favorites_playlist->count > 0)
        {
                save_named_playlist(expandedPath, favorites_playlist_name,
                                  favorites_playlist);
        }
}

void save_last_used_playlist(PlayList *unshuffled_playlist)
{
        char *configdir = get_config_path();
        save_named_playlist(configdir, last_used_playlist_name, unshuffled_playlist);
        if (configdir)
                free(configdir);
}

void save_playlist(const char *path, const PlayList *playlist)
{
        if (path == NULL)
        {
                return;
        }

        char expandedPath[MAXPATHLEN];
        expand_path(path, expandedPath);

        if (playlist->head == NULL || playlist->head->song.filePath == NULL)
                return;

        write_m3u_file(expandedPath, playlist);
}

void export_current_playlist(const char *path, const PlayList *playlist)
{
        char m3uFilename[MAXPATHLEN];
        char expandedPath[MAXPATHLEN];
        expand_path(path, expandedPath);

        if (path == NULL || playlist->head == NULL)
                return;

        generate_m3_u_filename(expandedPath, playlist->head->song.filePath, m3uFilename,
                            sizeof(m3uFilename));

        save_playlist(m3uFilename, playlist);
}

Node *deep_copy_node(Node *originalNode)
{
        if (originalNode == NULL)
        {
                return NULL;
        }

        Node *newNode = malloc(sizeof(Node));

        if (newNode == NULL)
        {
                return NULL;
        }

        newNode->song.filePath = strdup(originalNode->song.filePath);
        newNode->song.duration = originalNode->song.duration;
        newNode->prev = NULL;
        newNode->id = originalNode->id;
        newNode->next = deep_copy_node(originalNode->next);

        if (newNode->next != NULL)
        {
                newNode->next->prev = newNode;
        }

        return newNode;
}

Node *find_tail(Node *head)
{
        if (head == NULL)
                return NULL;

        Node *current = head;
        while (current->next != NULL)
        {
                current = current->next;
        }

        return current;
}

PlayList *deep_copy_play_list(const PlayList *originalList)
{
        if (originalList == NULL)
                return NULL;

        PlayList *newList = NULL;
        create_playlist(&newList);

        if (newList == NULL)
                return NULL;

        deep_copy_play_list_onto_list(originalList, &newList);
        return newList;
}

void deep_copy_play_list_onto_list(const PlayList *originalList, PlayList **newList)
{
        if (originalList == NULL || newList == NULL)
                return;

        if (*newList == NULL)
        {
                *newList = malloc(sizeof(PlayList));
                if (*newList == NULL)
                {
                        perror("malloc failed in deep_copy_play_list_onto_list");
                        return;
                }

                memset(*newList, 0, sizeof(PlayList));
                pthread_mutex_init(&(*newList)->mutex, NULL);
        }

        (*newList)->head = deep_copy_node(originalList->head);
        (*newList)->tail = find_tail((*newList)->head);
        (*newList)->count = originalList->count;
}

Node *find_path_in_playlist(const char *path, PlayList *playlist)
{
        Node *currentNode = playlist->head;

        while (currentNode != NULL)
        {
                if (strcmp(currentNode->song.filePath, path) == 0)
                {
                        return currentNode;
                }

                currentNode = currentNode->next;
        }
        return NULL;
}

Node *find_last_path_in_playlist(const char *path, PlayList *playlist)
{
        Node *currentNode = playlist->tail;

        while (currentNode != NULL)
        {
                if (strcmp(currentNode->song.filePath, path) == 0)
                {
                        return currentNode;
                }

                currentNode = currentNode->prev;
        }
        return NULL;
}

int find_node_in_list(PlayList *list, int id, Node **foundNode)
{
        Node *node = list->head;
        int row = 0;

        while (node != NULL)
        {
                if (id == node->id)
                {
                        *foundNode = node;
                        return row;
                }

                node = node->next;
                row++;
        }

        *foundNode = NULL;

        return -1;
}

void add_song_to_play_list(PlayList *list, const char *filePath, int playlistMax)
{
        if (list->count >= playlistMax)
                return;

        Node *newNode = NULL;
        create_node(&newNode, filePath, list->count);
        if (add_to_list(list, newNode) == -1)
                destroy_node(newNode);
}

void traverse_file_system_entry(FileSystemEntry *entry, PlayList *list,
                             int playlistMax)
{
        if (entry == NULL || list->count >= playlistMax)
                return;

        if (entry->is_directory == 0)
        {
                add_song_to_play_list(list, entry->fullPath, playlistMax);
        }

        if (entry->is_directory == 1 && entry->children != NULL)
        {
                traverse_file_system_entry(entry->children, list, playlistMax);
        }

        if (entry->next != NULL)
        {
                traverse_file_system_entry(entry->next, list, playlistMax);
        }
}

void create_play_list_from_file_system_entry(FileSystemEntry *root, PlayList *list,
                                       int playlistMax)
{
        traverse_file_system_entry(root, list, playlistMax);
}

int is_music_file(const char *filename)
{
        if (filename == NULL)
                return 0;

        const char *extensions[] = {".m4a", ".aac", ".mp3", ".ogg",
                                    ".flac", ".wav", ".opus", ".webm"};

        size_t numExtensions = sizeof(extensions) / sizeof(extensions[0]);

        for (size_t i = 0; i < numExtensions; i++)
        {
                if (strstr(filename, extensions[i]) != NULL)
                {
                        return 1;
                }
        }

        return 0;
}

int contains_music_files(FileSystemEntry *entry)
{
        if (entry == NULL)
                return 0;

        FileSystemEntry *child = entry->children;

        while (child != NULL)
        {
                if (!child->is_directory && is_music_file(child->name))
                {
                        return 1;
                }
                child = child->next;
        }

        return 0;
}

void add_album_to_play_list(PlayList *list, FileSystemEntry *album, int playlistMax)
{
        FileSystemEntry *entry = album->children;

        while (entry != NULL && list->count < playlistMax)
        {
                if (!entry->is_directory && is_music_file(entry->name))
                {
                        add_song_to_play_list(list, entry->fullPath, playlistMax);
                }
                entry = entry->next;
        }
}

void add_albums_to_play_list(FileSystemEntry *entry, PlayList *list,
                         int playlistMax)
{
        if (entry == NULL || list->count >= playlistMax)
                return;

        if (entry->is_directory && contains_music_files(entry))
        {
                add_album_to_play_list(list, entry, playlistMax);
        }

        if (entry->is_directory && entry->children != NULL)
        {
                add_albums_to_play_list(entry->children, list, playlistMax);
        }

        if (entry->next != NULL)
        {
                add_albums_to_play_list(entry->next, list, playlistMax);
        }
}

void shuffle_entries(FileSystemEntry **array, size_t n)
{
        if (n > 1)
        {
                for (size_t i = 0; i < n - 1; i++)
                {
                        size_t j = get_random_number(i, n - 1);

                        // Swap entries at i and j
                        FileSystemEntry *tmp = array[i];
                        array[i] = array[j];
                        array[j] = tmp;
                }
        }
}

void collect_albums(FileSystemEntry *entry, FileSystemEntry **albums,
                   size_t *count)
{
        if (entry == NULL)
                return;

        if (entry->is_directory && contains_music_files(entry))
        {
                albums[*count] = entry;
                (*count)++;
        }

        if (entry->is_directory && entry->children != NULL)
        {
                collect_albums(entry->children, albums, count);
        }

        if (entry->next != NULL)
        {
                collect_albums(entry->next, albums, count);
        }
}

void add_shuffled_albums_to_play_list(FileSystemEntry *root, PlayList *list,
                                 int playlistMax)
{
        size_t maxAlbums = 2000;
        FileSystemEntry *albums[maxAlbums];
        size_t albumCount = 0;

        collect_albums(root, albums, &albumCount);

        shuffle_entries(albums, albumCount);

        for (size_t i = 0; i < albumCount && list->count < playlistMax; i++)
        {
                add_album_to_play_list(list, albums[i], playlistMax);
        }
}

int increment_node_id() { return ++node_id_counter; }
