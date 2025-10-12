
#include "appstate.h"
#include "directorytree.h"
#include "player_ui.h"
#include <sys/stat.h>
#include <sys/time.h>
#include "common.h"
#include "term.h"
#include "utils.h"
#include "search_ui.h"  // FIXME: ui related stuff doesn't belong here
#include "playlist_ops.h"

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


// Go through the display playlist and the shuffle playlist to remove all songs
// except the current one. If no active song (if stopped rather than paused for
// example) entire playlist will be removed
void dequeueAllExceptPlayingSong(AppState *state)
{
        bool clearAll = false;
        int currentID = -1;

        Node *current = getCurrentSong();
        PlayList *unshuffledPlaylist = getUnshuffledPlaylist();
        PlayList *playlist = getPlaylist();

        // Do we need to clear the entire playlist?
        if (current == NULL)
        {
                clearAll = true;
        }
        else
        {
                currentID = current->id;
        }

        int nextInPlaylistID;
        pthread_mutex_lock(&(playlist->mutex));
        Node *songToBeRemoved;
        Node *nextInPlaylist = unshuffledPlaylist->head;

        while (nextInPlaylist != NULL)
        {
                nextInPlaylistID = nextInPlaylist->id;

                if (clearAll || nextInPlaylistID != currentID)
                {
                        songToBeRemoved = nextInPlaylist;

                        nextInPlaylist = nextInPlaylist->next;

                        int id = songToBeRemoved->id;

                        // Update Library
                        if (songToBeRemoved != NULL)
                                markAsDequeued(getLibrary(),
                                               songToBeRemoved->song.filePath);

                        // Remove from Display playlist
                        if (songToBeRemoved != NULL)
                                deleteFromList(unshuffledPlaylist,
                                               songToBeRemoved);

                        // Remove from Shuffle playlist
                        Node *node2 = findSelectedEntryById(playlist, id);
                        if (node2 != NULL)
                                deleteFromList(playlist, node2);
                }
                else
                {
                        nextInPlaylist = nextInPlaylist->next;
                }
        }
        pthread_mutex_unlock(&(playlist->mutex));

        PlaybackState *ps = getPlaybackState();
        ps->nextSongNeedsRebuilding = true;
        setNextSong(NULL);

        // Only refresh the screen if it makes sense to do so
        if (state->currentView == PLAYLIST_VIEW ||
            state->currentView == LIBRARY_VIEW)
        {
                triggerRefresh();
        }
}


void askIfCacheLibrary(UISettings *ui)
{
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

        setErrorMessage("Updating Library...");

        FileSystemEntry *tmp =
            createDirectoryTree(path, &tmpDirectoryTreeEntries);

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
        resetChosenDir();

        pthread_mutex_unlock(&(state->switchMutex));

        c_sleep(1000); // Don't refresh immediately or we risk the error message
                       // not clearing
        triggerRefresh();

        free(args->path);
        free(args);

        return NULL;
}

void updateLibrary(AppState *state, char *path)
{
        pthread_t threadId;

        freeSearchResults();

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

        if (path_stat->st_mtime != 0)
        {
                return path_stat->st_mtime;
        }
        else
        {
#ifdef __APPLE__
                return path_stat->st_mtimespec.tv_sec; // macOS-specific member.
#else
                return path_stat->st_mtim.tv_sec; // Linux-specific member.
#endif
        }
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
                updateLibrary(state, path);
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
                                updateLibrary(state, path);
                                break;
                        }
                }
        }

        closedir(dir);

        free(args);

        pthread_exit(NULL);
}

// This only checks the library mtime and toplevel subfolders mtimes
void updateLibraryIfChangedDetected(AppState *state)
{
        pthread_t tid;

        UpdateLibraryThreadArgs *args = malloc(sizeof(UpdateLibraryThreadArgs));

        if (args == NULL)
        {
                perror("malloc");
                return;
        }

        AppSettings *settings = getAppSettings();

        args->path = settings->path;
        args->state = state;

        if (pthread_create(&tid, NULL,
                           updateIfTopLevelFoldersMtimesChangedThread,
                           (void *)args) != 0)
        {
                perror("pthread_create");
                free(args);
        }
}

void createLibrary(AppSettings *settings, AppState *state)
{
        FileSystemEntry *library = getLibrary();

        if (state->uiSettings.cacheLibrary > 0)
        {
                char *libFilepath = getLibraryFilePath();
                library = reconstructTreeFromFile(
                    libFilepath, settings->path,
                    &(state->uiState.numDirectoryTreeEntries));
                free(libFilepath);
                updateLibraryIfChangedDetected(state);
        }

        if (library == NULL || library->children == NULL)
        {
                struct timeval start, end;

                gettimeofday(&start, NULL);

                library = createDirectoryTree(
                    settings->path, &(state->uiState.numDirectoryTreeEntries));

                gettimeofday(&end, NULL);
                long seconds = end.tv_sec - start.tv_sec;
                long microseconds = end.tv_usec - start.tv_usec;
                double elapsed = seconds + microseconds * 1e-6;

                // If time to load the library was significant, ask to use cache
                // instead
                if (elapsed > ASK_IF_USE_CACHE_LIMIT_SECONDS)
                {
                        askIfCacheLibrary(&(state->uiSettings));
                }
        }

        if (library == NULL || library->children == NULL)
        {
                char message[MAXPATHLEN + 64];

                snprintf(message, MAXPATHLEN + 64, "No music found at %s.",
                         settings->path);

                setErrorMessage(message);
        }
}
