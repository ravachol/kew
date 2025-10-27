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
#include "utils/term.h"
#include "utils/utils.h"

#include <sys/stat.h>
#include <sys/time.h>

#ifndef ASK_IF_USE_CACHE_LIMIT_SECONDS
#define ASK_IF_USE_CACHE_LIMIT_SECONDS 4
#endif

typedef struct
{
        char *path;
        AppState *state;
} UpdateLibraryThreadArgs;

int current_sort = 0;

void sort_library(void)
{
        FileSystemEntry *library = get_library();

        if (current_sort == 0)
        {
                sort_file_system_tree(library,
                                   compare_folders_by_age_files_alphabetically);
                current_sort = 1;
        }
        else
        {
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

        if (!root->is_directory)
        {
                if (strcmp(root->fullPath, path) == 0)
                {
                        root->isEnqueued = true;
                        return true;
                }
        }
        else
        {
                FileSystemEntry *child = root->children;
                bool found = false;
                while (child != NULL)
                {
                        found = mark_as_enqueued(child, path);
                        child = child->next;

                        if (found)
                                break;
                }

                if (found)
                {
                        root->isEnqueued = true;

                        return true;
                }
        }

        return false;
}

void mark_list_as_enqueued(FileSystemEntry *root, PlayList *playlist)
{
        Node *node = playlist->head;

        for (int i = 0; i < playlist->count; i++)
        {
                if (node == NULL)
                        break;

                if (node->song.filePath == NULL)
                        break;

                mark_as_enqueued(root, node->song.filePath);

                node = node->next;
        }

        root->isEnqueued = false; // Don't mark the absolute root
}

bool mark_as_dequeued(FileSystemEntry *root, char *path)
{
        int numChildrenEnqueued = 0;

        if (root == NULL)
                return false;

        if (!root->is_directory)
        {
                if (strcmp(root->fullPath, path) == 0)
                {
                        root->isEnqueued = false;
                        return true;
                }
        }
        else
        {
                FileSystemEntry *child = root->children;
                bool found = false;
                while (child != NULL)
                {
                        found = mark_as_dequeued(child, path);
                        child = child->next;

                        if (found)
                                break;
                }

                if (found)
                {
                        child = root->children;

                        while (child != NULL)
                        {
                                if (child->isEnqueued)
                                        numChildrenEnqueued++;

                                child = child->next;
                        }

                        if (numChildrenEnqueued == 0)
                                root->isEnqueued = false;

                        return true;
                }
        }

        return false;
}

void ask_if_cache_library()
{
        AppState *state = get_app_state();
        UISettings *ui = &(state->uiSettings);

        if (ui->cacheLibrary >
            -1) // Only use this function if cacheLibrary isn't set
                return;

        char input = '\0';

        restore_terminal_mode();
        enable_input_buffering();
        show_cursor();

        printf(_("Would you like to enable a (local) library cache for quicker "
               "startup "
               "times?\nYou can update the cache at any time by pressing 'u'. "
               "(y/n): "));

        fflush(stdout);

        do
        {
                int res = scanf(" %c", &input);

                if (res < 0)
                        break;

        } while (input != 'Y' && input != 'y' && input != 'N' && input != 'n');

        if (input == 'Y' || input == 'y')
        {
                printf("Y\n");
                ui->cacheLibrary = 1;
        }
        else
        {
                printf("N\n");
                ui->cacheLibrary = 0;
        }

        set_nonblocking_mode();
        set_raw_input_mode();
        hide_cursor();
}

typedef struct
{
        char *path;
        AppState *state;
} UpdateLibraryArgs;

void *update_library_thread(void *arg)
{
        if (arg == NULL)
                return NULL;

        UpdateLibraryArgs *args = arg;
        FileSystemEntry *library = get_library();

        char *path = args->path;
        AppState *state = args->state;
        int tmpDirectoryTreeEntries = 0;

        char expandedPath[MAXPATHLEN];
        expand_path(path, expandedPath);

        set_error_message("Updating Library...");

        FileSystemEntry *tmp =
            create_directory_tree(expandedPath, &tmpDirectoryTreeEntries);

        if (!tmp)
        {
                perror("create_directory_tree");
                pthread_mutex_unlock(&(state->switch_mutex));
                return NULL;
        }

        pthread_mutex_lock(&(state->switch_mutex));

        copy_is_enqueued(library, tmp);

        free_tree(library);
        library = tmp;
        state->uiState.numDirectoryTreeEntries = tmpDirectoryTreeEntries;

        pthread_mutex_unlock(&(state->switch_mutex));

        c_sleep(1000); // Don't refresh immediately or we risk the error message
                       // not clearing
        trigger_refresh();

        free(args->path);
        free(args);

        return NULL;
}

void update_library(char *path)
{
        AppState *state = get_app_state();
        pthread_t threadId;

        UpdateLibraryArgs *args = malloc(sizeof(UpdateLibraryArgs));

        if (!args)
                return; // handle allocation failure

        args->path = strdup(path);
        args->state = state;

        if (pthread_create(&threadId, NULL, update_library_thread, args) != 0)
        {
                perror("Failed to create thread");
                return;
        }
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

        if (stat(path, &path_stat) == -1)
        {
                free(args);
                pthread_exit(NULL);
        }

        if (get_modification_time(&path_stat) > ui->last_time_app_ran &&
            ui->last_time_app_ran > 0)
        {
                update_library(path);
                free(args);
                pthread_exit(NULL);
        }

        DIR *dir = opendir(path);
        if (!dir)
        {
                perror("opendir");
                free(args);
                pthread_exit(NULL);
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {

                if (strcmp(entry->d_name, ".") == 0 ||
                    strcmp(entry->d_name, "..") == 0)
                {
                        continue;
                }

                char fullPath[1024];
                snprintf(fullPath, sizeof(fullPath), "%s/%s", path,
                         entry->d_name);

                if (stat(fullPath, &path_stat) == -1)
                {
                        continue;
                }

                if (S_ISDIR(path_stat.st_mode))
                {

                        if (get_modification_time(&path_stat) >
                                ui->last_time_app_ran &&
                            ui->last_time_app_ran > 0)
                        {
                                update_library(path);
                                break;
                        }
                }
        }

        closedir(dir);

        free(args);

        pthread_exit(NULL);
}

// This only checks the library mtime and toplevel subfolders mtimes
void update_library_if_changed_detected(void)
{
        AppState *state = get_app_state();
        pthread_t tid;

        UpdateLibraryThreadArgs *args = malloc(sizeof(UpdateLibraryThreadArgs));

        if (args == NULL)
        {
                perror("malloc");
                return;
        }

        AppSettings *settings = get_app_settings();

        char expanded[MAXPATHLEN];

        expand_path(settings->path, expanded);

        args->path = expanded;
        args->state = state;

        if (pthread_create(&tid, NULL,
                           update_if_top_level_folders_mtimes_changed_thread,
                           (void *)args) != 0)
        {
                perror("pthread_create");
                free(args);
        }
}

void create_library()
{
        AppSettings *settings = get_app_settings();
        AppState *state = get_app_state();
        FileSystemEntry *library = get_library();

        if (state->uiSettings.cacheLibrary > 0)
        {
                char expanded[MAXPATHLEN];

                expand_path(settings->path, expanded);

                library = reconstruct_tree_from_file(
                    expanded, expanded,
                    &(state->uiState.numDirectoryTreeEntries));
                update_library_if_changed_detected();
        }

        if (library == NULL || library->children == NULL)
        {
                struct timeval start, end;

                gettimeofday(&start, NULL);

                char expanded[MAXPATHLEN];

                expand_path(settings->path, expanded);

                library = create_directory_tree(
                    expanded, &(state->uiState.numDirectoryTreeEntries));

                gettimeofday(&end, NULL);
                long seconds = end.tv_sec - start.tv_sec;
                long microseconds = end.tv_usec - start.tv_usec;
                double elapsed = seconds + microseconds * 1e-6;

                // If time to load the library was significant, ask to use cache
                // instead
                if (elapsed > ASK_IF_USE_CACHE_LIMIT_SECONDS)
                {
                        ask_if_cache_library();
                }
        }

        if (library == NULL || library->children == NULL)
        {
                char message[MAXPATHLEN + 64];

                snprintf(message, MAXPATHLEN + 64, "No music found at %s.",
                         settings->path);

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
        create_node(&node, child->fullPath, id);
        if (add_to_list(unshuffled_playlist, node) == -1)
                destroy_node(node);

        Node *node2 = NULL;
        create_node(&node2, child->fullPath, id);
        if (add_to_list(playlist, node2) == -1)
                destroy_node(node2);

        child->isEnqueued = 1;
        child->parent->isEnqueued = 1;
}

void dequeue_song(FileSystemEntry *child)
{
        PlayList *unshuffled_playlist = get_unshuffled_playlist();
        PlayList *playlist = get_playlist();

        Node *node1 =
            find_last_path_in_playlist(child->fullPath, unshuffled_playlist);

        Node *current = get_current_song();

        if (node1 == NULL)
                return;

        if (current != NULL && current->id == node1->id)
        {
                remove_currently_playing_song();
        }
        else
        {
                if (get_song_to_start_from() != NULL)
                {
                        set_song_to_start_from(get_list_next(node1));
                }
        }

        int id = node1->id;

        Node *node2 = find_selected_entry_by_id(playlist, id);

        if (node1 != NULL)
                delete_from_list(unshuffled_playlist, node1);

        if (node2 != NULL)
                delete_from_list(playlist, node2);

        child->isEnqueued = 0;

        // Check if parent needs to be dequeued as well
        bool isEnqueued = false;

        FileSystemEntry *ch = child->parent->children;

        while (ch != NULL)
        {
                if (ch->isEnqueued)
                {
                        isEnqueued = true;
                        break;
                }
                ch = ch->next;
        }

        if (!isEnqueued)
        {
                child->parent->isEnqueued = 0;
                if (child->parent->parent != NULL)
                        child->parent->parent->isEnqueued = 0;
        }
}

void dequeue_children(FileSystemEntry *parent)
{
        FileSystemEntry *child = parent->children;

        while (child != NULL)
        {
                if (child->is_directory && child->children != NULL)
                {
                        dequeue_children(child);
                }
                else
                {
                        dequeue_song(child);
                }

                child = child->next;
        }
}

int enqueue_children(FileSystemEntry *child,
                     FileSystemEntry **firstEnqueuedEntry)
{
        int hasEnqueued = 0;

        while (child != NULL)
        {
                if (child->is_directory && child->children != NULL)
                {
                        child->isEnqueued = enqueue_children(child->children, firstEnqueuedEntry);

                        if (child->isEnqueued == 1)
                                hasEnqueued = 1;
                }
                else if (!child->isEnqueued)
                {
                        if (*firstEnqueuedEntry == NULL)
                                *firstEnqueuedEntry = child;

                        if (!(path_ends_with(child->fullPath, "m3u") ||
                                path_ends_with(child->fullPath, "m3u8")))
                        {
                                enqueue_song(child);
                                hasEnqueued = 1;
                        }
                }

                child = child->next;
        }
        return hasEnqueued;
}

bool has_song_children(FileSystemEntry *entry)
{
        if (!entry)
                return false;

        FileSystemEntry *child = entry->children;
        int numSongs = 0;

        while (child != NULL)
        {
                if (!child->is_directory)
                        numSongs++;

                child = child->next;
        }

        if (numSongs == 0)
        {
                return false;
        }

        return true;
}

bool has_dequeued_children(FileSystemEntry *parent)
{
        FileSystemEntry *child = parent->children;

        bool isDequeued = false;
        while (child != NULL)
        {
                if (!child->isEnqueued)
                {
                        if ((child->is_directory != 1 && !(path_ends_with(child->fullPath, "m3u") ||
                                path_ends_with(child->fullPath, "m3u8"))) || has_dequeued_children(child))
                                isDequeued = true;
                }
                child = child->next;
        }

        return isDequeued;
}

bool is_contained_within(FileSystemEntry *entry, FileSystemEntry *containingEntry)
{
        if (entry == NULL || containingEntry == NULL)
                return false;

        FileSystemEntry *tmp = entry->parent;

        while (tmp != NULL)
        {
                if (strcmp(tmp->fullPath, containingEntry->fullPath) == 0)
                        return true;

                tmp = tmp->parent;
        }

        return false;
}
