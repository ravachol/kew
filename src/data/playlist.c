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

static char search[MAX_SEARCH_SIZE];
static char playlist_name[MAX_SEARCH_SIZE];
static bool shuffle = false;
static int num_dirs = 0;
static Node *current_song = NULL;
static int node_id_counter = 0;

void clear_current_song(void)
{
        current_song = NULL;
}

void set_current_song(Node *node)
{
        current_song = node;
}

Node *get_current_song(void)
{
        return current_song;
}

Node *get_list_next(Node *node)
{
        return (node == NULL) ? NULL : node->next;
}

Node *get_list_prev(Node *node)
{
        return (node == NULL) ? NULL : node->prev;
}

int add_to_list(PlayList *list, Node *new_node)
{
        if (new_node == NULL)
                return 0;

        if (list == NULL || list->count >= MAX_FILES)
                return -1;

        list->count++;

        if (list->head == NULL) {
                new_node->prev = NULL;
                list->head = new_node;
                list->tail = new_node;
        } else {
                new_node->prev = list->tail;
                list->tail->next = new_node;
                list->tail = new_node;
        }

        return 0;
}

void move_up_list(PlayList *list, Node *node)
{
        if (node == list->head || node == NULL || node->prev == NULL)
                return;

        Node *prev_node = node->prev;
        Node *next_node = node->next;

        if (prev_node->prev)
                prev_node->prev->next = node;
        else
                list->head = node;

        node->prev = prev_node->prev;
        node->next = prev_node;

        prev_node->prev = node;
        prev_node->next = next_node;

        if (next_node)
                next_node->prev = prev_node;
        else
                list->tail = prev_node;
}

void move_down_list(PlayList *list, Node *node)
{
        if (node == list->tail || node == NULL || node->next == NULL)
                return;

        Node *next_node = node->next;
        Node *prev_node = node->prev;
        Node *next_next_node = next_node->next;

        if (prev_node)
                prev_node->next = next_node;
        else
                list->head = next_node;

        next_node->prev = prev_node;
        next_node->next = node;

        node->prev = next_node;
        node->next = next_next_node;

        if (next_next_node)
                next_next_node->prev = node;
        else
                list->tail = node;
}

Node *delete_from_list(PlayList *list, Node *node)
{
        if (list == NULL || node == NULL || list->head == NULL)
                return NULL;

        Node *next_node = node->next;

        // Adjust head and tail
        if (list->head == node)
                list->head = next_node;
        if (list->tail == node)
                list->tail = node->prev;

        // Fix neighbors
        if (node->prev != NULL)
                node->prev->next = next_node;
        if (next_node != NULL)
                next_node->prev = node->prev;

        // Free song file path string if allocated
        if (node->song.file_path != NULL) {
                free(node->song.file_path);
                node->song.file_path = NULL;
        }

        free(node);
        node = NULL;

        list->count--;

        return next_node;
}

void empty_playlist(PlayList *list)
{
        if (list == NULL)
                return;

        pthread_mutex_lock(&list->mutex);

        Node *current = list->head;
        while (current != NULL) {
                Node *next = current->next;
                free(current->song.file_path);
                free(current);
                current = next;
        }

        list->head = NULL;
        list->tail = NULL;
        list->count = 0;

        pthread_mutex_unlock(&list->mutex);
}

void shuffle_playlist(PlayList *playlist)
{
        if (playlist == NULL || playlist->count <= 1) {
                return; // No need to shuffle
        }

        // Check for overflow before malloc
        if ((size_t)playlist->count > SIZE_MAX / sizeof(Node *)) {
                printf(_("Playlist too large to allocate.\n"));
                // atexit() will free up resources properly
                exit(1);
        }

        // Convert the linked list to an array
        Node **nodes = (Node **)malloc(playlist->count * sizeof(Node *));
        if (nodes == NULL) {
                printf(_("Memory allocation error.\n"));
                // atexit() will free up resources properly
                exit(1);
        }

        Node *current = playlist->head;
        int i = 0;
        while (current != NULL) {
                nodes[i++] = current;
                current = current->next;
        }

        // Shuffle the array using Fisher-Yates algorithm
        for (int j = playlist->count - 1; j >= 1; --j) {
                int k = rand() % (j + 1);
                Node *tmp = nodes[j];
                nodes[j] = nodes[k];
                nodes[k] = tmp;
        }

        playlist->head = nodes[0];
        playlist->tail = nodes[playlist->count - 1];
        for (int j = 0; j < playlist->count; ++j) {
                nodes[j]->next =
                    (j < playlist->count - 1) ? nodes[j + 1] : NULL;
                nodes[j]->prev = (j > 0) ? nodes[j - 1] : NULL;
        }
        free(nodes);
}

void insert_as_first(Node *current_song, PlayList *playlist)
{
        if (current_song == NULL || playlist == NULL) {
                return;
        }

        if (playlist->head == NULL) {
                current_song->next = NULL;
                current_song->prev = NULL;
                playlist->head = current_song;
                playlist->tail = current_song;
        } else {
                if (current_song != playlist->head) {
                        if (current_song->next != NULL) {
                                current_song->next->prev = current_song->prev;
                        } else {
                                playlist->tail = current_song->prev;
                        }

                        if (current_song->prev != NULL) {
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
        if (song != NULL && playlist->count > 1) {
                insert_as_first(song, playlist);
        }
}

void create_node(Node **node, const char *directory_path, int id)
{
        SongInfo song;
        song.file_path = strdup(directory_path);
        song.duration = 0.0;

        *node = (Node *)malloc(sizeof(Node));
        if (*node == NULL) {
                printf(_("Failed to allocate memory."));
                free(song.file_path);
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

        if (node->song.file_path != NULL)
                free(node->song.file_path);

        free(node);
}

void exit_if_overflow(int counter)
{
        if (counter == INT_MAX) {
                fprintf(stderr,
                        "Error: Node ID overflow. Max node limit reached.\n");
                exit(1);
        }
}

void build_playlist_recursive(const char *directory_path,
                              const char *allowed_extensions, PlayList *playlist)
{
        char expanded_path[PATH_MAX - NAME_MAX - 1];
        expand_path(directory_path, expanded_path);

        int res = is_directory(expanded_path);
        if (res != 1 && res != -1 && directory_path != NULL) {
                Node *node = NULL;

                exit_if_overflow(node_id_counter);
                create_node(&node, expanded_path, node_id_counter++);
                if (add_to_list(playlist, node) == -1)
                        destroy_node(node);

                return;
        }

        DIR *dir = opendir(expanded_path);
        if (dir == NULL) {
                printf(_("Failed to open directory: %s\n"), expanded_path);
                return;
        }

        regex_t regex;
        int ret = regcomp(&regex, allowed_extensions, REG_EXTENDED | REG_ICASE);
        if (ret != 0) {
                printf(_("Failed to compile regular expression\n"));
                closedir(dir);
                return;
        }

        char exto[100];
        struct dirent **entries;
        int num_entries =
            scandir(expanded_path, &entries, NULL, compare_lib_entries);

        if (num_entries < 0) {
                printf(_("Failed to scan directory: %s\n"), expanded_path);
                return;
        }

        for (int i = 0; i < num_entries && playlist->count < MAX_FILES; i++) {
                struct dirent *entry = entries[i];

                if (entry->d_name[0] == '.' ||
                    strcmp(entry->d_name, ".") == 0 ||
                    strcmp(entry->d_name, "..") == 0) {
                        continue;
                }

                char file_path[PATH_MAX];
                snprintf(file_path, sizeof(file_path), "%s/%s", expanded_path,
                         entry->d_name);

                size_t pathLen = strnlen(expanded_path, sizeof(expanded_path));
                size_t nameLen = strnlen(entry->d_name, NAME_MAX);

                if (pathLen + 1 + nameLen >= PATH_MAX) {
                        fprintf(stderr, "Path too long: %s/%s\n", expanded_path, entry->d_name);
                        return; // or skip this entry
                }

                snprintf(file_path, sizeof(file_path), "%s/%s", expanded_path, entry->d_name);

                if (is_directory(file_path) == 1) {
                        int song_count = playlist->count;
                        build_playlist_recursive(file_path, allowed_extensions,
                                                 playlist);
                        if (playlist->count > song_count)
                                num_dirs++;
                } else {
                        extract_extension(entry->d_name, sizeof(exto) - 1, exto);
                        if (match_regex(&regex, exto) == 0) {
                                snprintf(file_path, sizeof(file_path), "%s/%s",
                                         directory_path, entry->d_name);

                                Node *node = NULL;

                                exit_if_overflow(node_id_counter);
                                create_node(&node, file_path, node_id_counter++);
                                if (add_to_list(playlist, node) == -1)
                                        destroy_node(node);
                        }
                }
        }

        for (int i = 0; i < num_entries; i++) {
                free(entries[i]);
        }
        free(entries);

        closedir(dir);
        regfree(&regex);
}

int join_playlist(PlayList *dest, PlayList *src)
{
        if (src->count == 0) {
                return 0;
        }

        if (dest->count == 0) {
                dest->head = src->head;
                dest->tail = src->tail;
        } else {
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

void make_playlist_name(const char *search, int max_size)
{
        playlist_name[0] = '\0';

        snprintf(playlist_name, max_size, "%s", search);

        snprintf(playlist_name + strnlen(playlist_name, max_size),
                 max_size - strnlen(playlist_name, max_size), ".m3u");

        for (int i = 0; playlist_name[i] != '\0'; i++) {
                if (playlist_name[i] == ':') {
                        playlist_name[i] = '-';
                }
        }
}

Node *read_m3u_file(const char *filepath, PlayList *playlist)
{
        GError *error = NULL;
        gchar *contents;
        gchar **lines;
        Node *first_in_list = NULL;

        char filename[PATH_MAX];
        expand_path(filepath, filename);

        if (!g_file_get_contents(filename, &contents, NULL, &error)) {
                g_clear_error(&error);
                return NULL;
        }

        gchar *directory = g_path_get_dirname(filename);

        lines = g_strsplit(contents, "\n", -1);

        for (gint i = 0; lines[i] != NULL; i++) {
                gchar *line = lines[i];

                line = g_strdelimit(line, "\r", '\0');
                gchar *trimmed_line = g_strstrip(line);

                if (trimmed_line[0] != '#' && trimmed_line[0] != '\0') {
                        gchar *songPath;

                        if (g_path_is_absolute(trimmed_line)) {
                                songPath = g_strdup(trimmed_line);
                        } else {
                                songPath = g_build_filename(directory,
                                                            trimmed_line, NULL);
                        }

                        if (songPath == NULL)
                                continue;

                        if (exists_file(songPath) < 0) {
                                g_free(songPath);
                                continue;
                        }
                        // Don't add songs that are already enqueued
                        Node *found = find_path_in_playlist(songPath, playlist);

                        if (first_in_list == NULL)
                                first_in_list = found;

                        if (found != NULL) {
                                g_free(songPath);
                                continue;
                        }

                        Node *new_node = NULL;
                        node_id_counter++;
                        create_node(&new_node, songPath, node_id_counter);

                        if (playlist->head == NULL) {
                                playlist->head = new_node;
                                playlist->tail = new_node;
                        } else {
                                playlist->tail->next = new_node;
                                new_node->prev = playlist->tail;
                                playlist->tail = new_node;
                        }

                        if (first_in_list == NULL)
                                first_in_list = playlist->tail;

                        playlist->count++;

                        g_free(songPath);
                }
        }

        g_free(directory);
        g_strfreev(lines);
        g_free(contents);

        return first_in_list;
}

int make_playlist(PlayList **playlist, int argc, char *argv[], bool exact_search, const char *path)
{
        const char *delimiter = ":";
        const char *allowed_extensions = MUSIC_FILE_EXTENSIONS;

        enum SearchType search_type = SearchAny;
        int search_type_index = 1;
        PlayList partial_playlist = {NULL, NULL, 0, PTHREAD_MUTEX_INITIALIZER};

        char expanded_path[PATH_MAX];
        expand_path(path, expanded_path);

        if (strcmp(argv[1], "all") == 0) {
                search_type = ReturnAllSongs;
                shuffle = true;
        }

        if (argc > 1) {
                if (strcmp(argv[1], "list") == 0 && argc > 2) {
                        allowed_extensions = PLAYLIST_EXTENSIONS;
                        search_type = SearchPlayList;
                }

                if (strcmp(argv[1], "random") == 0 ||
                    strcmp(argv[1], "rand") == 0 ||
                    strcmp(argv[1], "shuffle") == 0) {
                        int count = 0;
                        while (argv[count] != NULL) {
                                count++;
                        }
                        if (count > 2) {
                                search_type_index = 2;
                                shuffle = true;
                        }
                }

                if (strcmp(argv[search_type_index], "dir") == 0)
                        search_type = DirOnly;
                else if (strcmp(argv[search_type_index], "song") == 0)
                        search_type = FileOnly;
        }

        int start = search_type_index + 1;

        if (search_type == FileOnly || search_type == DirOnly ||
            search_type == SearchPlayList)
                start = search_type_index + 2;

        search[0] = '\0';

        for (int i = start - 1; i < argc; i++) {
                size_t len = strnlen(search, MAX_SEARCH_SIZE);
                snprintf(search + len, MAX_SEARCH_SIZE - len, " %s", argv[i]);
        }

        make_playlist_name(search, MAX_SEARCH_SIZE);

        if (strstr(search, delimiter)) {
                shuffle = true;
        }

        if (search_type == ReturnAllSongs) {
                pthread_mutex_lock(&((*playlist)->mutex));

                build_playlist_recursive(expanded_path, allowed_extensions, *playlist);

                pthread_mutex_unlock(&((*playlist)->mutex));
        } else {
                char *token = strtok(search, delimiter);

                while (token != NULL) {
                        char buf[PATH_MAX] = {0};
                        if (strncmp(token, "song", 4) == 0) {
                                memmove(token, token + 4,
                                        strnlen(token + 4, PATH_MAX) + 1);
                                search_type = FileOnly;
                        }
                        trim(token, PATH_MAX);
                        char *searching = g_utf8_casefold(token, -1);

                        if (walker(expanded_path, searching, buf, allowed_extensions,
                                   search_type, exact_search, 0) == 0) {
                                if (strcmp(argv[1], "list") == 0) {
                                        read_m3u_file(buf, *playlist);
                                } else {
                                        pthread_mutex_lock(&((*playlist)->mutex));

                                        build_playlist_recursive(buf, allowed_extensions, &partial_playlist);

                                        join_playlist(*playlist, &partial_playlist);

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

void generate_m3_u_filename(const char *base_path, const char *file_path,
                            char *m3u_filename, size_t size)
{

        const char *base_name = strrchr(file_path, '/');
        if (base_name == NULL) {
                base_name = file_path; // No '/' found, use the entire filename
        } else {
                base_name++; // Skip the '/' character
        }

        const char *dot = strrchr(base_name, '.');
        if (dot == NULL) {
                // No '.' found, copy the base name and append ".m3u"
                if (base_path[strnlen(base_path, PATH_MAX) - 1] == '/') {
                        snprintf(m3u_filename, size, "%s%s.m3u", base_path, base_name);
                } else {
                        snprintf(m3u_filename, size, "%s/%s.m3u", base_path, base_name);
                }
        } else {
                // Copy the base name up to the dot and append ".m3u"
                size_t baseNameLen = dot - base_name;
                if (base_path[strnlen(base_path, PATH_MAX) - 1] == '/') {
                        snprintf(m3u_filename, size, "%s%.*s.m3u", base_path,
                                 (int)baseNameLen, base_name);
                } else {
                        snprintf(m3u_filename, size, "%s/%.*s.m3u", base_path,
                                 (int)baseNameLen, base_name);
                }
        }
}

void write_m3u_file(const char *filename, const PlayList *playlist)
{
        FILE *file = fopen(filename, "w");
        if (file == NULL) {
                return;
        }

        Node *current_node = playlist->head;
        while (current_node != NULL) {
                fprintf(file, "%s\n", current_node->song.file_path);
                current_node = current_node->next;
        }
        fclose(file);
}

void load_playlist(const char *directory, const char *playlist_name,
                   PlayList **playlist)
{
        char playlist_path[PATH_MAX];

        if (!directory || !playlist_name || !playlist)
                return;

        size_t len = strnlen(directory, PATH_MAX);
        if (len == 0)
                return;

        snprintf(playlist_path, sizeof(playlist_path), "%s%s%s",
                 directory,
                 (directory[len - 1] == '/' ? "" : "/"),
                 playlist_name);

        if (*playlist == NULL) {
                create_playlist(playlist);
        }

        if (*playlist)
                read_m3u_file(playlist_path, *playlist);
}

void load_favorites_playlist(const char *directory, PlayList **favorites_playlist)
{
        char expanded_path[PATH_MAX];
        expand_path(directory, expanded_path);
        load_playlist(directory, favorites_playlist_name, favorites_playlist);
        set_favorites_playlist(*favorites_playlist);
}

void add_enqueued_songs_to_playlist(FileSystemEntry *root, PlayList *playlist)
{
        if (!root)
                return;

        if (root->is_enqueued && root->is_directory == 0) {
                Node *node = malloc(sizeof(Node));
                node->song.file_path = strdup(root->full_path);
                node->prev = playlist->tail;
                node->next = NULL;
                node->id = root->id;
                node->song.duration = 0.0;

                if (playlist->tail)
                        playlist->tail->next = node;
                else
                        playlist->head = node;

                playlist->tail = node;
                playlist->count++;
        }

        for (FileSystemEntry *child = root->children; child; child = child->next)
                add_enqueued_songs_to_playlist(child, playlist);
}

void save_named_playlist(const char *directory, const char *playlist_name,
                         const PlayList *playlist)
{
        if (directory == NULL) {
                return;
        }

        char playlist_path[PATH_MAX];

        int length =
            snprintf(playlist_path, sizeof(playlist_path), "%s", directory);

        if (length <= 0 || length >= (int)sizeof(playlist_path) ||
            playlist_path[0] == '\0') {
                return;
        }

        if (playlist_path[length - 1] != '/') {
                snprintf(playlist_path + length, sizeof(playlist_path) - length,
                         "/%s", playlist_name);
        } else {
                snprintf(playlist_path + length, sizeof(playlist_path) - length,
                         "%s", playlist_name);
        }

        if (playlist != NULL) {
                write_m3u_file(playlist_path, playlist);
        }
}

void save_favorites_playlist(const char *directory, PlayList *favorites_playlist)
{
        char expanded_path[PATH_MAX];
        expand_path(directory, expanded_path);

        if (favorites_playlist != NULL && favorites_playlist->count > 0) {
                save_named_playlist(expanded_path, favorites_playlist_name,
                                    favorites_playlist);
        }
}

void save_playlist(const char *path, const PlayList *playlist)
{
        if (path == NULL) {
                return;
        }

        char expanded_path[PATH_MAX];
        expand_path(path, expanded_path);

        if (playlist->head == NULL || playlist->head->song.file_path == NULL)
                return;

        write_m3u_file(expanded_path, playlist);
}

void export_current_playlist(const char *path, const PlayList *playlist)
{
        char m3u_filename[PATH_MAX];
        char expanded_path[PATH_MAX];
        expand_path(path, expanded_path);

        if (path == NULL || playlist->head == NULL)
                return;

        generate_m3_u_filename(expanded_path, playlist->head->song.file_path, m3u_filename,
                               sizeof(m3u_filename));

        save_playlist(m3u_filename, playlist);
}

Node *deep_copy_node(Node *original_node)
{
        if (original_node == NULL) {
                return NULL;
        }

        Node *new_node = malloc(sizeof(Node));

        if (new_node == NULL) {
                return NULL;
        }

        new_node->song.file_path = strdup(original_node->song.file_path);
        new_node->song.duration = original_node->song.duration;
        new_node->prev = NULL;
        new_node->id = original_node->id;
        new_node->next = deep_copy_node(original_node->next);

        if (new_node->next != NULL) {
                new_node->next->prev = new_node;
        }

        return new_node;
}

Node *find_tail(Node *head)
{
        if (head == NULL)
                return NULL;

        Node *current = head;
        while (current->next != NULL) {
                current = current->next;
        }

        return current;
}

PlayList *deep_copy_playlist(const PlayList *original_list)
{
        if (original_list == NULL)
                return NULL;

        PlayList *new_list = NULL;
        create_playlist(&new_list);

        if (new_list == NULL)
                return NULL;

        deep_copy_play_list_onto_list(original_list, &new_list);
        return new_list;
}

void deep_copy_play_list_onto_list(const PlayList *original_list, PlayList **new_list)
{
        if (original_list == NULL || new_list == NULL)
                return;

        if (*new_list == NULL) {
                *new_list = malloc(sizeof(PlayList));
                if (*new_list == NULL) {
                        perror("malloc failed in deep_copy_play_list_onto_list");
                        return;
                }

                memset(*new_list, 0, sizeof(PlayList));
                pthread_mutex_init(&(*new_list)->mutex, NULL);
        }
        else if ((*new_list)->count > 0) {
                empty_playlist(*new_list);
        }

        (*new_list)->head = deep_copy_node(original_list->head);
        (*new_list)->tail = find_tail((*new_list)->head);
        (*new_list)->count = original_list->count;
}

Node *find_path_in_playlist(const char *path, PlayList *playlist)
{
        if (!playlist)
                return NULL;

        Node *current_node = playlist->head;

        while (current_node != NULL) {
                if (strcmp(current_node->song.file_path, path) == 0) {
                        return current_node;
                }

                current_node = current_node->next;
        }
        return NULL;
}

Node *find_last_path_in_playlist(const char *path, PlayList *playlist)
{
        if (!playlist)
                return NULL;

        Node *current_node = playlist->tail;

        while (current_node != NULL) {
                if (strcmp(current_node->song.file_path, path) == 0) {
                        return current_node;
                }

                current_node = current_node->prev;
        }
        return NULL;
}

int find_node_in_list(PlayList *list, int id, Node **found_node)
{
        Node *node = list->head;
        int row = 0;

        while (node != NULL) {
                if (id == node->id) {
                        *found_node = node;
                        return row;
                }

                node = node->next;
                row++;
        }

        *found_node = NULL;

        return -1;
}

void add_song_to_play_list(PlayList *list, const char *file_path, int playlist_max)
{
        if (list->count >= playlist_max)
                return;

        Node *new_node = NULL;
        create_node(&new_node, file_path, list->count);
        if (add_to_list(list, new_node) == -1)
                destroy_node(new_node);
}

void traverse_file_system_entry(FileSystemEntry *entry, PlayList *list,
                                int playlist_max)
{
        if (entry == NULL || list->count >= playlist_max)
                return;

        if (entry->is_directory == 0) {
                add_song_to_play_list(list, entry->full_path, playlist_max);
        }

        if (entry->is_directory == 1 && entry->children != NULL) {
                traverse_file_system_entry(entry->children, list, playlist_max);
        }

        if (entry->next != NULL) {
                traverse_file_system_entry(entry->next, list, playlist_max);
        }
}

void create_play_list_from_file_system_entry(FileSystemEntry *root, PlayList *list,
                                             int playlist_max)
{
        traverse_file_system_entry(root, list, playlist_max);
}

int is_music_file(const char *filename)
{
        if (filename == NULL)
                return 0;

        const char *extensions[] = {".m4a", ".aac", ".mp3", ".ogg",
                                    ".flac", ".wav", ".opus", ".webm"};

        size_t numExtensions = sizeof(extensions) / sizeof(extensions[0]);

        for (size_t i = 0; i < numExtensions; i++) {
                if (strstr(filename, extensions[i]) != NULL) {
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

        while (child != NULL) {
                if (!child->is_directory && is_music_file(child->name)) {
                        return 1;
                }
                child = child->next;
        }

        return 0;
}

void add_album_to_play_list(PlayList *list, FileSystemEntry *album, int playlist_max)
{
        FileSystemEntry *entry = album->children;

        while (entry != NULL && list->count < playlist_max) {
                if (!entry->is_directory && is_music_file(entry->name)) {
                        add_song_to_play_list(list, entry->full_path, playlist_max);
                }
                entry = entry->next;
        }
}

void add_albums_to_play_list(FileSystemEntry *entry, PlayList *list,
                             int playlist_max)
{
        if (entry == NULL || list->count >= playlist_max)
                return;

        if (entry->is_directory && contains_music_files(entry)) {
                add_album_to_play_list(list, entry, playlist_max);
        }

        if (entry->is_directory && entry->children != NULL) {
                add_albums_to_play_list(entry->children, list, playlist_max);
        }

        if (entry->next != NULL) {
                add_albums_to_play_list(entry->next, list, playlist_max);
        }
}

void shuffle_entries(FileSystemEntry **array, size_t n)
{
        if (n > 1) {
                for (size_t i = 0; i < n - 1; i++) {
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

        if (entry->is_directory && contains_music_files(entry)) {
                albums[*count] = entry;
                (*count)++;
        }

        if (entry->is_directory && entry->children != NULL) {
                collect_albums(entry->children, albums, count);
        }

        if (entry->next != NULL) {
                collect_albums(entry->next, albums, count);
        }
}

void add_shuffled_albums_to_play_list(FileSystemEntry *root, PlayList *list,
                                      int playlist_max)
{
        size_t maxAlbums = 2000;
        FileSystemEntry *albums[maxAlbums];
        size_t albumCount = 0;

        collect_albums(root, albums, &albumCount);

        shuffle_entries(albums, albumCount);

        for (size_t i = 0; i < albumCount && list->count < playlist_max; i++) {
                add_album_to_play_list(list, albums[i], playlist_max);
        }
}

int increment_node_id()
{
        return ++node_id_counter;
}
