/**
 * @file library_ops.h
 * @brief Music library management and scanning operations.
 *
 * Responsible for reading directories, and updating the in-memory library
 * representation used by playlists
 * and the UI browser.
 */

#include "common/appstate.h"
#include "common/common.h"

#include "playlist_ops.h"
#include "track_manager.h"

#include "data/directorytree.h"

#include "utils/file.h"
#include "utils/utils.h"

#include <sys/stat.h>
#include <sys/time.h>

typedef struct
{
        char *path;
        bool wait_until_complete;
        AppState *state;
} UpdateLibraryThreadArgs;

static int current_sort = 0;

static bool updating_library = false;

void reset_sort_library(void)
{
        FileSystemEntry *library = get_library();

        if (current_sort == 1) {
                sort_file_system_tree(library, compare_entry_natural);
                current_sort = 0;
        }
}

void sort_library(void)
{
        FileSystemEntry *library = get_library();

        if (current_sort == 0) {
                sort_file_system_tree(library,
                                      compare_folders_by_age_files_alphabetically);
                current_sort = 1;
        } else {
                sort_file_system_tree(library, compare_entry_natural);
                current_sort = 0;
        }

        trigger_refresh();
}

bool mark_as_enqueued(FileSystemEntry *root, char *path)
{
        if (root == NULL)
                return false;

        if (path == NULL)
                return false;

        if (!root->is_directory) {
                if (strcmp(root->full_path, path) == 0) {
                        root->is_enqueued = true;
                        return true;
                }
        } else {
                FileSystemEntry *child = root->children;
                bool found = false;
                while (child != NULL) {
                        found = mark_as_enqueued(child, path);
                        child = child->next;

                        if (found)
                                break;
                }

                if (found) {
                        root->is_enqueued = true;

                        return true;
                }
        }

        return false;
}

void mark_list_as_enqueued(FileSystemEntry *root, PlayList *playlist)
{
        Node *node = playlist->head;

        for (int i = 0; i < playlist->count; i++) {
                if (node == NULL)
                        break;

                if (node->song.file_path == NULL)
                        break;

                mark_as_enqueued(root, node->song.file_path);

                node = node->next;
        }

        root->is_enqueued = false; // Don't mark the absolute root
}

bool mark_as_dequeued(FileSystemEntry *root, char *path)
{
        int num_children_enqueued = 0;

        if (root == NULL)
                return false;

        if (!root->is_directory) {
                if (strcmp(root->full_path, path) == 0) {
                        root->is_enqueued = false;
                        return true;
                }
        } else {
                FileSystemEntry *child = root->children;
                bool found = false;
                while (child != NULL) {
                        found = mark_as_dequeued(child, path);
                        child = child->next;

                        if (found)
                                break;
                }

                if (found) {
                        child = root->children;

                        while (child != NULL) {
                                if (child->is_enqueued)
                                        num_children_enqueued++;

                                child = child->next;
                        }

                        if (num_children_enqueued == 0)
                                root->is_enqueued = false;

                        return true;
                }
        }

        return false;
}

typedef struct
{
        char *path;
        AppState *state;
} UpdateLibraryArgs;

void *update_library_thread(void *arg)
{
        if (arg == NULL || updating_library)
                return NULL;

        updating_library = true;

        UpdateLibraryArgs *args = arg;
        FileSystemEntry *library = get_library();

        char *path = args->path;
        AppState *state = args->state;
        int tmp_directory_tree_entries = 0;

        char expanded_path[PATH_MAX];
        expand_path(path, expanded_path);

        FileSystemEntry *tmp =
            create_directory_tree(expanded_path, &tmp_directory_tree_entries);

        if (!tmp) {
                perror("create_directory_tree");
                pthread_mutex_unlock(&(state->switch_mutex));
                free(args->path);
                free(args);
                updating_library = false;
                return NULL;
        }

        pthread_mutex_lock(&(state->switch_mutex));

        copy_is_enqueued(library, tmp);

        set_library(tmp);
        free_tree(library);

        state->uiState.numDirectoryTreeEntries = tmp_directory_tree_entries;

        pthread_mutex_unlock(&(state->switch_mutex));

        c_sleep(1000); // Don't refresh immediately or we risk the error message
                       // not clearing
        trigger_refresh();

        free(args->path);
        free(args);
        updating_library = false;

        return NULL;
}

void update_library(char *path, bool wait_until_complete)
{
        AppState *state = get_app_state();
        pthread_t thread_id;

        UpdateLibraryArgs *args = malloc(sizeof(UpdateLibraryArgs));

        if (!args)
                return; // handle allocation failure

        args->path = strdup(path);
        args->state = state;

        if (pthread_create(&thread_id, NULL, update_library_thread, args) != 0) {
                perror("Failed to create thread");
                return;
        }

        if (wait_until_complete)
                pthread_join(thread_id, NULL);

        state->uiSettings.last_time_app_ran = time(NULL);
}

time_t get_modification_time(struct stat *path_stat)
{
        return path_stat->st_mtime;
}

void *update_if_top_level_folders_mtimes_changed_thread(void *arg)
{
        UpdateLibraryThreadArgs *args = (UpdateLibraryThreadArgs *)
            arg; // Cast `arg` back to the structure pointer
        char *path = args->path;

        AppState *state = args->state;
        UISettings *ui = &(state->uiSettings);

        struct stat path_stat;

        if (stat(path, &path_stat) == -1) {
                if (args->path)
                        free(args->path);
                free(args);
                pthread_exit(NULL);
        }

        if (get_modification_time(&path_stat) > ui->last_time_app_ran &&
            ui->last_time_app_ran > 0) {
                update_library(path, args->wait_until_complete);
                if (args->path)
                        free(args->path);
                free(args);
                pthread_exit(NULL);
        }

        DIR *dir = opendir(path);
        if (!dir) {
                perror("opendir");
                if (args->path)
                        free(args->path);
                pthread_exit(NULL);
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {

                if (strcmp(entry->d_name, ".") == 0 ||
                    strcmp(entry->d_name, "..") == 0) {
                        continue;
                }

                char full_path[1024];
                snprintf(full_path, sizeof(full_path), "%s/%s", path,
                         entry->d_name);

                if (stat(full_path, &path_stat) == -1) {
                        continue;
                }

                if (S_ISDIR(path_stat.st_mode)) {

                        if (get_modification_time(&path_stat) >
                                ui->last_time_app_ran &&
                            ui->last_time_app_ran > 0) {
                                update_library(path, args->wait_until_complete);
                                break;
                        }
                }
        }

        closedir(dir);

        if (args->path)
                free(args->path);
        free(args);

        pthread_exit(NULL);
}

// This only checks the library mtime and toplevel subfolders mtimes
void update_library_if_changed_detected(bool wait_until_complete)
{
        AppState *state = get_app_state();
        pthread_t tid;

        UpdateLibraryThreadArgs *args = malloc(sizeof(UpdateLibraryThreadArgs));
        if (args == NULL) {
                perror("malloc");
                return;
        }

        AppSettings *settings = get_app_settings();

        char expanded[PATH_MAX];
        expand_path(settings->path, expanded);

        args->path = strdup(expanded);
        if (args->path == NULL) {
                perror("strdup");
                free(args);
                return;
        }

        args->wait_until_complete = wait_until_complete;
        args->state = state;

        if (pthread_create(&tid, NULL,
                           update_if_top_level_folders_mtimes_changed_thread,
                           (void *)args) != 0) {
                perror("pthread_create");
                free(args->path);
                free(args);
        }

        if (wait_until_complete)
                pthread_join(tid, NULL);
}

void create_library(bool set_enqueued_status)
{
        AppSettings *settings = get_app_settings();
        AppState *state = get_app_state();
        FileSystemEntry *library = NULL;

        char expanded[PATH_MAX];

        expand_path(settings->path, expanded);

        char *lib_path = get_library_file_path();

        library = read_tree_from_binary(
            lib_path, expanded,
            &(state->uiState.numDirectoryTreeEntries), set_enqueued_status);

        free(lib_path);

        set_library(library);

        bool wait_until_complete = true;
        update_library_if_changed_detected(wait_until_complete);

        library = get_library();

        bool library_path_changed = false;
        if (library && strcmp(library->full_path, expanded) != 0)
                library_path_changed = true;

        if (library == NULL || library->children == NULL || library_path_changed) {

                char expanded[PATH_MAX];

                expand_path(settings->path, expanded);

                library = create_directory_tree(expanded, &(state->uiState.numDirectoryTreeEntries));
        }

        if (library == NULL || library->children == NULL) {
                char message[PATH_MAX + 64];

                snprintf(message, PATH_MAX + 64, "No music found at %s.", settings->path);

                set_error_message(message);
        }

        set_library(library);
}

void enqueue_song(FileSystemEntry *child)
{
        int id = increment_node_id();

        PlayList *unshuffled_playlist = get_unshuffled_playlist();
        PlayList *playlist = get_playlist();

        Node *node = NULL;
        create_node(&node, child->full_path, id);
        if (add_to_list(unshuffled_playlist, node) == -1)
                destroy_node(node);

        Node *node2 = NULL;
        create_node(&node2, child->full_path, id);
        if (add_to_list(playlist, node2) == -1)
                destroy_node(node2);

        child->is_enqueued = 1;
        child->parent->is_enqueued = 1;
}

void set_childrens_queued_status_on_parents(FileSystemEntry *parent, bool wanted_status)
{
        if (parent == NULL)
                return;

        bool is_enqueued = false;

        FileSystemEntry *ch = parent->children;

        while (ch != NULL) {
                if (ch->is_enqueued) {
                        is_enqueued = true;
                        break;
                }
                ch = ch->next;
        }

        if (is_enqueued == wanted_status) {
                parent->is_enqueued = wanted_status;
        }

        parent = parent->parent;

        if (parent && parent->parent != NULL)
                set_childrens_queued_status_on_parents(parent, wanted_status);
}

void dequeue_song(FileSystemEntry *child)
{
        PlayList *unshuffled_playlist = get_unshuffled_playlist();
        PlayList *playlist = get_playlist();

        Node *node1 =
            find_last_path_in_playlist(child->full_path, unshuffled_playlist);

        Node *current = get_current_song();

        if (node1 == NULL)
                return;

        if (current != NULL && current->id == node1->id) {
                remove_currently_playing_song();
        } else {
                if (get_song_to_start_from() != NULL) {
                        set_song_to_start_from(get_list_next(node1));
                }
        }

        int id = node1->id;

        Node *node2 = find_selected_entry_by_id(playlist, id);

        if (node1 != NULL)
                delete_from_list(unshuffled_playlist, node1);

        if (node2 != NULL)
                delete_from_list(playlist, node2);

        child->is_enqueued = 0;

}

void dequeue_children(FileSystemEntry *parent)
{
        FileSystemEntry *child = parent->children;

        while (child != NULL) {
                if (child->is_directory && child->children != NULL) {
                        dequeue_children(child);
                } else {
                        dequeue_song(child);
                }

                child = child->next;
        }

        set_childrens_queued_status_on_parents(parent, false);
}

int enqueue_children(FileSystemEntry *child,
                     FileSystemEntry **first_enqueued_entry)
{
        int has_enqueued = 0;

        if (!child)
                return has_enqueued;

        FileSystemEntry *parent = child->parent;

        while (child != NULL) {
                if (child->is_directory && child->children != NULL) {
                        child->is_enqueued = enqueue_children(child->children, first_enqueued_entry);

                        if (child->is_enqueued == 1)
                                has_enqueued = 1;
                } else if (!child->is_enqueued) {
                        if (*first_enqueued_entry == NULL)
                                *first_enqueued_entry = child;

                        if (!(path_ends_with(child->full_path, "m3u") ||
                              path_ends_with(child->full_path, "m3u8"))) {
                                enqueue_song(child);
                                has_enqueued = 1;
                        }
                }
                else if (child->is_directory == 0 && child->is_enqueued)
                {
                        has_enqueued = 1;
                }

                child = child->next;
        }

        set_childrens_queued_status_on_parents(parent, true);

        return has_enqueued;
}

bool has_song_children(FileSystemEntry *entry)
{
        if (!entry)
                return false;

        FileSystemEntry *child = entry->children;
        int num_songs = 0;

        while (child != NULL) {
                if (!child->is_directory)
                        num_songs++;

                child = child->next;
        }

        if (num_songs == 0) {
                return false;
        }

        return true;
}

bool has_dequeued_children(FileSystemEntry *parent)
{
        FileSystemEntry *child = parent->children;

        bool isDequeued = false;
        while (child != NULL) {
                if (!child->is_enqueued) {
                        if ((child->is_directory != 1 && !(path_ends_with(child->full_path, "m3u") ||
                                                           path_ends_with(child->full_path, "m3u8"))) ||
                            has_dequeued_children(child))
                                isDequeued = true;
                }
                child = child->next;
        }

        return isDequeued;
}

bool is_contained_within(FileSystemEntry *entry, FileSystemEntry *containing_entry)
{
        if (entry == NULL || containing_entry == NULL)
                return false;

        FileSystemEntry *tmp = entry->parent;

        while (tmp != NULL) {
                if (strcmp(tmp->full_path, containing_entry->full_path) == 0)
                        return true;

                tmp = tmp->parent;
        }

        return false;
}
