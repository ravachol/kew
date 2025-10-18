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
#include "trackmanager.h"

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

int currentSort = 0;

void sortLibrary(void)
{
        FileSystemEntry *library = getLibrary();

        if (currentSort == 0)
        {
                sortFileSystemTree(library,
                                   compareFoldersByAgeFilesAlphabetically);
                currentSort = 1;
        }
        else
        {
                sortFileSystemTree(library, compareEntryNatural);
                currentSort = 0;
        }

        triggerRefresh();
}

bool markAsEnqueued(FileSystemEntry *root, char *path)
{
        if (root == NULL)
                return false;

        if (path == NULL)
                return false;

        if (!root->isDirectory)
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
                        found = markAsEnqueued(child, path);
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

void markListAsEnqueued(FileSystemEntry *root, PlayList *playlist)
{
        Node *node = playlist->head;

        for (int i = 0; i < playlist->count; i++)
        {
                if (node == NULL)
                        break;

                if (node->song.filePath == NULL)
                        break;

                markAsEnqueued(root, node->song.filePath);

                node = node->next;
        }

        root->isEnqueued = false; // Don't mark the absolute root
}

bool markAsDequeued(FileSystemEntry *root, char *path)
{
        int numChildrenEnqueued = 0;

        if (root == NULL)
                return false;

        if (!root->isDirectory)
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
                        found = markAsDequeued(child, path);
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

void askIfCacheLibrary()
{
        AppState *state = getAppState();
        UISettings *ui = &(state->uiSettings);

        if (ui->cacheLibrary >
            -1) // Only use this function if cacheLibrary isn't set
                return;

        char input = '\0';

        restoreTerminalMode();
        enableInputBuffering();
        showCursor();

        printf("Would you like to enable a (local) library cache for quicker "
               "startup "
               "times?\nYou can update the cache at any time by pressing 'u'. "
               "(y/n): ");

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

        setNonblockingMode();
        setRawInputMode();
        hideCursor();
}

typedef struct
{
        char *path;
        AppState *state;
} UpdateLibraryArgs;

void *updateLibraryThread(void *arg)
{
        if (arg == NULL)
                return NULL;

        UpdateLibraryArgs *args = arg;
        FileSystemEntry *library = getLibrary();

        char *path = args->path;
        AppState *state = args->state;
        int tmpDirectoryTreeEntries = 0;

        char expandedPath[MAXPATHLEN];
        expandPath(path, expandedPath);

        setErrorMessage("Updating Library...");

        FileSystemEntry *tmp =
            createDirectoryTree(expandedPath, &tmpDirectoryTreeEntries);

        if (!tmp)
        {
                perror("createDirectoryTree");
                pthread_mutex_unlock(&(state->switchMutex));
                return NULL;
        }

        pthread_mutex_lock(&(state->switchMutex));

        copyIsEnqueued(library, tmp);

        freeTree(library);
        library = tmp;
        state->uiState.numDirectoryTreeEntries = tmpDirectoryTreeEntries;

        pthread_mutex_unlock(&(state->switchMutex));

        c_sleep(1000); // Don't refresh immediately or we risk the error message
                       // not clearing
        triggerRefresh();

        free(args->path);
        free(args);

        return NULL;
}

void updateLibrary(char *path)
{
        AppState *state = getAppState();
        pthread_t threadId;

        UpdateLibraryArgs *args = malloc(sizeof(UpdateLibraryArgs));

        if (!args)
                return; // handle allocation failure

        args->path = strdup(path);
        args->state = state;

        if (pthread_create(&threadId, NULL, updateLibraryThread, args) != 0)
        {
                perror("Failed to create thread");
                return;
        }
}

time_t getModificationTime(struct stat *path_stat)
{
    return path_stat->st_mtime;
}

void *updateIfTopLevelFoldersMtimesChangedThread(void *arg)
{
        UpdateLibraryThreadArgs *args = (UpdateLibraryThreadArgs *)
            arg; // Cast `arg` back to the structure pointer
        char *path = args->path;
        AppState *state = args->state;
        UISettings *ui = &(state->uiSettings);

        struct stat path_stat;

        if (stat(path, &path_stat) == -1)
        {
                perror("stat");
                free(args);
                pthread_exit(NULL);
        }

        if (getModificationTime(&path_stat) > ui->lastTimeAppRan &&
            ui->lastTimeAppRan > 0)
        {
                updateLibrary(path);
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
                        perror("stat");
                        continue;
                }

                if (S_ISDIR(path_stat.st_mode))
                {

                        if (getModificationTime(&path_stat) >
                                ui->lastTimeAppRan &&
                            ui->lastTimeAppRan > 0)
                        {
                                updateLibrary(path);
                                break;
                        }
                }
        }

        closedir(dir);

        free(args);

        pthread_exit(NULL);
}

// This only checks the library mtime and toplevel subfolders mtimes
void updateLibraryIfChangedDetected(void)
{
        AppState *state = getAppState();
        pthread_t tid;

        UpdateLibraryThreadArgs *args = malloc(sizeof(UpdateLibraryThreadArgs));

        if (args == NULL)
        {
                perror("malloc");
                return;
        }

        AppSettings *settings = getAppSettings();

        char expanded[MAXPATHLEN];

        expandPath(settings->path, expanded);

        args->path = expanded;
        args->state = state;

        if (pthread_create(&tid, NULL,
                           updateIfTopLevelFoldersMtimesChangedThread,
                           (void *)args) != 0)
        {
                perror("pthread_create");
                free(args);
        }
}

void createLibrary()
{
        AppSettings *settings = getAppSettings();
        AppState *state = getAppState();
        FileSystemEntry *library = getLibrary();

        if (state->uiSettings.cacheLibrary > 0)
        {
                char expanded[MAXPATHLEN];

                expandPath(settings->path, expanded);

                library = reconstructTreeFromFile(
                    expanded, expanded,
                    &(state->uiState.numDirectoryTreeEntries));
                updateLibraryIfChangedDetected();
        }

        if (library == NULL || library->children == NULL)
        {
                struct timeval start, end;

                gettimeofday(&start, NULL);

                char expanded[MAXPATHLEN];

                expandPath(settings->path, expanded);

                library = createDirectoryTree(
                    expanded, &(state->uiState.numDirectoryTreeEntries));

                gettimeofday(&end, NULL);
                long seconds = end.tv_sec - start.tv_sec;
                long microseconds = end.tv_usec - start.tv_usec;
                double elapsed = seconds + microseconds * 1e-6;

                // If time to load the library was significant, ask to use cache
                // instead
                if (elapsed > ASK_IF_USE_CACHE_LIMIT_SECONDS)
                {
                        askIfCacheLibrary();
                }
        }

        if (library == NULL || library->children == NULL)
        {
                char message[MAXPATHLEN + 64];

                snprintf(message, MAXPATHLEN + 64, "No music found at %s.",
                         settings->path);

                setErrorMessage(message);
        }

        setLibrary(library);
}

void enqueueSong(FileSystemEntry *child)
{
        int id = incrementNodeId();

        PlayList *unshuffledPlaylist = getUnshuffledPlaylist();
        PlayList *playlist = getPlaylist();

        Node *node = NULL;
        createNode(&node, child->fullPath, id);
        if (addToList(unshuffledPlaylist, node) == -1)
                destroyNode(node);

        Node *node2 = NULL;
        createNode(&node2, child->fullPath, id);
        if (addToList(playlist, node2) == -1)
                destroyNode(node2);

        child->isEnqueued = 1;
        child->parent->isEnqueued = 1;
}

void dequeueSong(FileSystemEntry *child)
{
        PlayList *unshuffledPlaylist = getUnshuffledPlaylist();
        PlayList *playlist = getPlaylist();

        Node *node1 =
            findLastPathInPlaylist(child->fullPath, unshuffledPlaylist);

        Node *current = getCurrentSong();

        if (node1 == NULL)
                return;

        if (current != NULL && current->id == node1->id)
        {
                removeCurrentlyPlayingSong();
        }
        else
        {
                if (getSongToStartFrom() != NULL)
                {
                        setSongToStartFrom(getListNext(node1));
                }
        }

        int id = node1->id;

        Node *node2 = findSelectedEntryById(playlist, id);

        if (node1 != NULL)
                deleteFromList(unshuffledPlaylist, node1);

        if (node2 != NULL)
                deleteFromList(playlist, node2);

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

void dequeueChildren(FileSystemEntry *parent)
{
        FileSystemEntry *child = parent->children;

        while (child != NULL)
        {
                if (child->isDirectory && child->children != NULL)
                {
                        dequeueChildren(child);
                }
                else
                {
                        dequeueSong(child);
                }

                child = child->next;
        }
}

void enqueueChildren(FileSystemEntry *child,
                     FileSystemEntry **firstEnqueuedEntry)
{
        while (child != NULL)
        {
                if (child->isDirectory && child->children != NULL)
                {
                        child->isEnqueued = 1;
                        enqueueChildren(child->children, firstEnqueuedEntry);
                }
                else if (!child->isEnqueued)
                {
                        if (*firstEnqueuedEntry == NULL)
                                *firstEnqueuedEntry = child;
                        enqueueSong(child);
                }

                child = child->next;
        }
}

bool hasSongChildren(FileSystemEntry *entry)
{
        if (!entry)
                return false;

        FileSystemEntry *child = entry->children;
        int numSongs = 0;

        while (child != NULL)
        {
                if (!child->isDirectory)
                        numSongs++;

                child = child->next;
        }

        if (numSongs == 0)
        {
                return false;
        }

        return true;
}

bool hasDequeuedChildren(FileSystemEntry *parent)
{
        FileSystemEntry *child = parent->children;

        bool isDequeued = false;
        while (child != NULL)
        {
                if (!child->isEnqueued)
                {
                        isDequeued = true;
                }
                child = child->next;
        }

        return isDequeued;
}

bool isContainedWithin(FileSystemEntry *entry, FileSystemEntry *containingEntry)
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
