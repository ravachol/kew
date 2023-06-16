#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <regex.h>
#include <string.h>
#include <time.h>
#include "playlist.h"
#include "dir.h"
#include "stringfunc.h"
#include "settings.h"

const int MAX_FILES = 4096;
const char ALLOWED_EXTENSIONS[] = "\\.(m4a|mp3|ogg|flac|wav|aac|wma|raw|mp4a|mp4)$";
PlayList playlist = {NULL, NULL};
volatile int stopThread = 0;

Node *getListNext(Node *node)
{
    return (node == NULL) ? NULL : node->next;
}

Node *getListPrev(Node *node)
{
    return (node == NULL) ? NULL : node->prev;
}

void addToList(PlayList *list, SongInfo song)
{
    if (list->count >= MAX_FILES)
        return;
    Node *newNode = (Node *)malloc(sizeof(Node));
    newNode->song = song;
    newNode->next = NULL;
    list->count++;

    if (list->head == NULL)
    {
        // The list is empty
        newNode->prev = NULL;
        list->head = newNode;
        list->tail = newNode;
    }
    else
    {
        // Append the new node at the end of the list
        newNode->prev = list->tail;
        list->tail->next = newNode;
        list->tail = newNode;
    }
}

void shufflePlaylist(PlayList *playlist)
{
    if (playlist == NULL || playlist->count <= 1)
    {
        return; // No need to shuffle
    }

    // Convert the linked list to an array
    Node **nodes = (Node **)malloc(playlist->count * sizeof(Node *));
    if (nodes == NULL)
    {
        printf("Memory allocation error.\n");
        return;
    }

    Node *current = playlist->head;
    int i = 0;
    while (current != NULL)
    {
        nodes[i++] = current;
        current = current->next;
    }

    // Shuffle the array using Fisher-Yates algorithm
    srand(time(NULL));
    for (int j = playlist->count - 1; j > 0; --j)
    {
        int k = rand() % (j + 1);
        Node *temp = nodes[j];
        nodes[j] = nodes[k];
        nodes[k] = temp;
    }

    // Update the playlist with the shuffled order
    playlist->head = nodes[0];
    playlist->tail = nodes[playlist->count - 1];
    for (int j = 0; j < playlist->count; ++j)
    {
        nodes[j]->next = (j < playlist->count - 1) ? nodes[j + 1] : NULL;
        nodes[j]->prev = (j > 0) ? nodes[j - 1] : NULL;
    }
    free(nodes);
}

int compare(const struct dirent **a, const struct dirent **b)
{
    const char *nameA = (*a)->d_name;
    const char *nameB = (*b)->d_name;

    if (nameA[0] == '_' && nameB[0] != '_')
    {
        return -1; // Directory A starts with underscore, so it should come first
    }
    else if (nameA[0] != '_' && nameB[0] == '_')
    {
        return 1; // Directory B starts with underscore, so it should come first
    }

    return strcmp(nameA, nameB); // Lexicographic comparison for other cases
}

void buildPlaylistRecursive(char *directoryPath, const char *allowedExtensions, PlayList *playlist)
{
    if (!isDirectory(directoryPath) && directoryPath != NULL)
    {
        SongInfo song;
        song.filePath = strdup(directoryPath);
        song.duration = 0.0;
        addToList(playlist, song);
        return;
    }

    DIR *dir = opendir(directoryPath);
    if (dir == NULL)
    {
        printf("Failed to open directory: %s\n", directoryPath);
        return;
    }

    regex_t regex;
    int ret = regcomp(&regex, allowedExtensions, REG_EXTENDED);
    if (ret != 0)
    {
        printf("Failed to compile regular expression\n");
        closedir(dir);
        return;
    }

    char exto[6]; // +1 for null-terminator
    struct dirent **entries;
    int numEntries = scandir(directoryPath, &entries, NULL, compare);

    if (numEntries < 0)
    {
        printf("Failed to scan directory: %s\n", directoryPath);
        return;
    }

    for (int i = 0; i < numEntries && playlist->count < MAX_FILES; i++)
    {
        struct dirent *entry = entries[i];

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue; // Skip current and parent directory entries
        }

        char filePath[FILENAME_MAX];
        snprintf(filePath, sizeof(filePath), "%s/%s", directoryPath, entry->d_name);

        if (isDirectory(filePath))
        {
            // Entry is a directory, recursively process it
            buildPlaylistRecursive(filePath, allowedExtensions, playlist);
        }
        else
        {
            // Entry is a regular file, check if it matches the desired file format
            extractExtension(entry->d_name, sizeof(exto) - 1, exto);
            if (match_regex(&regex, exto) == 0)
            {
                SongInfo song;
                snprintf(filePath, sizeof(filePath), "%s/%s", directoryPath, entry->d_name);
                song.filePath = strdup(filePath); // Allocate memory and copy the file path
                song.duration = 0.0;
                addToList(playlist, song);
            }
        }
    }

    closedir(dir);
    regfree(&regex);
}

int playDirectory(const char *directoryPath, const char *allowedExtensions, PlayList *playlist)
{
    DIR *dir = opendir(directoryPath);
    if (dir == NULL)
    {
        printf("Failed to open directory: %s\n", directoryPath);
        return -1;
    }

    regex_t regex;
    int ret = regcomp(&regex, allowedExtensions, REG_EXTENDED);
    if (ret != 0)
    {
        return -1;
    }
    char ext[6]; // +1 for null-terminator, should be able to handle .flac
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        extractExtension(entry->d_name, sizeof(ext) - 1, ext);
        if (match_regex(&regex, ext) == 0)
        {
            char filePath[FILENAME_MAX];
            snprintf(filePath, sizeof(filePath), "%s/%s", directoryPath, entry->d_name);
            SongInfo song;
            song.filePath = strdup(filePath); // Allocate memory and copy the file path
            addToList(playlist, song);
        }
    }
    closedir(dir);

    return 0;
}

int joinPlaylist(PlayList *dest, PlayList *src)
{
    if (src->count == 0)
    {
        return 0; // Nothing to join if playlistB is empty
    }

    if (dest->count == 0)
    {
        // If playlistA is empty, simply update its head and tail
        dest->head = src->head;
        dest->tail = src->tail;
    }
    else
    {
        // Update tail of playlistA and head of playlistB
        dest->tail->next = src->head;
        src->head->prev = dest->tail;
        dest->tail = src->tail;
    }

    // Update the count of playlistA
    dest->count += src->count;

    // Reset playlistB
    src->head = NULL;
    src->tail = NULL;
    src->count = 0;

    return 1; // Successful join
}

int makePlaylist(int argc, char *argv[])
{
    enum SearchType searchType = SearchAny;
    int searchTypeIndex = 1;
    bool shuffle = false;
    const char *delimiter = ":";
    PlayList partialPlaylist = {NULL, NULL};

    if (strcmp(argv[1], "random") == 0 || strcmp(argv[1], "rand") == 0 || strcmp(argv[1], "shuffle") == 0)
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

    int start = searchTypeIndex + 1;

    if (searchType == FileOnly || searchType == DirOnly)
        start = searchTypeIndex + 2;

    // create search string
    int size = 256;
    char search[size];
    strncpy(search, argv[start - 1], size - 1);
    for (int i = start; i < argc; i++)
    {
        strcat(search, " ");
        strcat(search, argv[i]);
    }

    if (strstr(search, delimiter))
    {
        if (searchType == SearchAny)
            searchType = DirOnly;
        shuffle = true;
    }

    char *token = strtok(search, delimiter);

    // Subsequent calls to strtok with NULL to continue splitting
    while (token != NULL)
    {
        char buf[MAXPATHLEN] = {0};
        char path[MAXPATHLEN] = {0};
        if (strncmp(token, "song", 4) == 0)
        {
            // Remove "dir" from token by shifting characters
            memmove(token, token + 4, strlen(token + 4) + 1);
            searchType = FileOnly;
        }
        trim(token);
        if (walker(settings.path, token, buf, ALLOWED_EXTENSIONS, searchType) == 0)
        {
            buildPlaylistRecursive(buf, ALLOWED_EXTENSIONS, &partialPlaylist);
            joinPlaylist(&playlist, &partialPlaylist);
        }

        token = strtok(NULL, delimiter);
        searchType = DirOnly;
    }

    if (shuffle)
        shufflePlaylist(&playlist);

    if (playlist.head == NULL)
        puts("Music not found");

    return 0;
}

void *getDurationsThread(void *arg)
{
    // Thread code goes here
    PlayList *playList = (PlayList *)arg;

    if (playList == NULL)
        return NULL;

    Node *currentNode = playList->head;
    double totalDuration = 0.0;

    for (int i = 0; i < playList->count; i++)
    {
        if (stopThread)
            return NULL;

        if (currentNode == NULL)
            return NULL;

        if (currentNode->song.duration > 0.0)
        {
            currentNode = getListNext(currentNode);
            continue;
        }
        if (currentNode->song.filePath == NULL)
        {
            currentNode = getListNext(currentNode);
            continue;
        }
        char musicFilepath[MAX_FILENAME_LENGTH];
        strcpy(musicFilepath, currentNode->song.filePath);
        double duration = ((currentNode->song.duration > 0.0) ? currentNode->song.duration : getDuration(musicFilepath));
        if (duration > 0.0)
        {
            currentNode->song.duration = duration;
            totalDuration += duration;
            playList->totalDuration += duration;
        }
        currentNode = getListNext(currentNode);
        usleep(10000);
    }

    // Recount them all
    currentNode = playList->head;
    playList->totalDuration = 0.0;
    for (int i = 0; i < playList->count; i++)
    {
        if (currentNode != NULL && currentNode->song.duration > 0.0)
            playList->totalDuration += currentNode->song.duration;
        currentNode = getListNext(currentNode);
    }
    return NULL;
}

int setPlayListDurations(PlayList *playlist)
{
    if (playlist->totalDuration > 0.0)
        return 0;

    pthread_t thread;
    int threadCreationResult;

    // Create a new thread
    threadCreationResult = pthread_create(&thread, NULL, getDurationsThread, (void *)playlist);
    if (threadCreationResult != 0)
    {
        return 1;
    }

    return 0;
}