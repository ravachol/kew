#define _XOPEN_SOURCE 700
#define __USE_XOPEN_EXTENDED 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <regex.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "cutils.h"
#include "playlist.h"
#include "file.h"
#include "stringfunc.h"
#include "settings.h"

#define MAX_SEARCH_SIZE 256
#define MAX_FILES 20000

const char ALLOWED_EXTENSIONS[] = "\\.(m4a|mp3|ogg|flac|wav|aac|wma|raw|mp4a|mp4|opus)$";
const char PLAYLIST_EXTENSIONS[] = "\\.(m3u)$";
const char mainPlaylistName[] = "cue.m3u";
PlayList playlist = {NULL, NULL, 0, 0.0};
PlayList *mainPlaylist = NULL;
PlayList *originalPlaylist = NULL;


char search[MAX_SEARCH_SIZE];
char playlistName[MAX_SEARCH_SIZE];
bool shuffle = false;
int numDirs = 0;
volatile int stopPlaylistDurationThread = 0;
Node *currentSong = NULL;

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
}

Node *deleteFromList(PlayList *list, Node *node)
{
    if (list->head == NULL || node == NULL)
        return NULL;

    if (node == list->head)
        list->head = node->next;
    if (node == list->tail)
        list->tail = node->prev;

    if (node->prev != NULL)
        node->prev->next = node->next;
    if (node->next != NULL)
        node->next->prev = node->prev;

    Node *nextNode = node->next;

    free(node);
    list->count--;
    return nextNode;
}

void deletePlaylist(PlayList *list)
{
    if (list == NULL)
        return;

    Node *current = list->head;
    while (current != NULL)
    {
        Node *next = current->next;
        if (current->song.filePath != NULL)
            free(current->song.filePath);
        free(current);
        current = next;
    }

    // Reset the playlist
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
    list->totalDuration = 0.0;
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
        exit(0);
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
    for (int j = playlist->count - 1; j >= 1; --j)
    {
        int k = rand() % (j + 1);
        Node *temp = nodes[j];
        nodes[j] = nodes[k];
        nodes[k] = temp;
    }
    playlist->head = nodes[0];
    playlist->tail = nodes[playlist->count - 1];
    for (int j = 0; j < playlist->count; ++j)
    {
        nodes[j]->next = (j < playlist->count - 1) ? nodes[j + 1] : NULL;
        nodes[j]->prev = (j > 0) ? nodes[j - 1] : NULL;
    }
    free(nodes);
}

void insertAsFirst(Node* currentSong, PlayList* playlist) {
    if (currentSong == NULL || playlist == NULL) {
        return;
    }

    if (playlist->head == NULL) {
        currentSong->next = NULL;
        currentSong->prev = NULL;
        playlist->head = currentSong;
        playlist->tail = currentSong;
    } else {
        if (currentSong != playlist->head) {            
            if (currentSong->next != NULL) {
                currentSong->next->prev = currentSong->prev;
            }
            if (currentSong->prev != NULL) {
                currentSong->prev->next = currentSong->next;
            }

            // Add the currentSong as the new head
            currentSong->next = playlist->head;
            currentSong->prev = NULL;
            playlist->head->prev = currentSong;
            playlist->head = currentSong;
        }
    }
}

void shufflePlaylistStartingFromSong(PlayList *playlist, Node *song)
{
    shufflePlaylist(playlist);
    if( playlist->count > 1)
        insertAsFirst(song, playlist);
}

int compare(const struct dirent **a, const struct dirent **b)
{
    const char *nameA = (*a)->d_name;
    const char *nameB = (*b)->d_name;

    if (nameA[0] == '_' && nameB[0] != '_')
    {
        return -1;
    }
    else if (nameA[0] != '_' && nameB[0] == '_')
    {
        return 1;
    }

    return strcmp(nameA, nameB);
}

void buildPlaylistRecursive(char *directoryPath, const char *allowedExtensions, PlayList *playlist)
{
    int res = isDirectory(directoryPath);
    if (res != 1 && res != -1 && directoryPath != NULL)
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

    char exto[6];
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
            continue;
        }

        char filePath[FILENAME_MAX];
        snprintf(filePath, sizeof(filePath), "%s/%s", directoryPath, entry->d_name);

        if (isDirectory(filePath))
        {
            int songCount = playlist->count;
            buildPlaylistRecursive(filePath, allowedExtensions, playlist);
            if (playlist->count > songCount)
                numDirs++;
        }
        else
        {
            extractExtension(entry->d_name, sizeof(exto) - 1, exto);
            if (match_regex(&regex, exto) == 0)
            {
                SongInfo song;
                snprintf(filePath, sizeof(filePath), "%s/%s", directoryPath, entry->d_name);
                song.filePath = strdup(filePath);
                song.duration = 0.0;
                addToList(playlist, song);
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
    char ext[6];
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        extractExtension(entry->d_name, sizeof(ext) - 1, ext);
        if (match_regex(&regex, ext) == 0)
        {
            char filePath[FILENAME_MAX];
            snprintf(filePath, sizeof(filePath), "%s/%s", directoryPath, entry->d_name);
            SongInfo song;
            song.duration = 0.0;
            song.filePath = strdup(filePath);
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

void makePlaylistName(const char *search)
{
    char *duplicateSearch = strdup(search);
    strcat(playlistName, duplicateSearch);
    free(duplicateSearch);
    strcat(playlistName, ".m3u");
    int i = 0;
    while (playlistName[i] != '\0')
    {
        if (playlistName[i] == ':')
        {
            playlistName[i] = '-';
        }
        i++;
    }
}

int makePlaylist(int argc, char *argv[])
{
    enum SearchType searchType = SearchAny;
    int searchTypeIndex = 1;

    const char *delimiter = ":";
    PlayList partialPlaylist = {NULL, NULL, 0, 0.0};

    const char *allowedExtensions = ALLOWED_EXTENSIONS;

    if (argc == 1)
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
    }
    int start = searchTypeIndex + 1;

    if (searchType == FileOnly || searchType == DirOnly || searchType == SearchPlayList)
        start = searchTypeIndex + 2;

    search[0] = '\0';

    for (int i = start - 1; i < argc; i++)
    {
        strcat(search, " ");
        strcat(search, argv[i]);
    }
    makePlaylistName(search);

    if (strstr(search, delimiter))
    {
        shuffle = true;
    }

    if (searchType == ReturnAllSongs)
    {
        buildPlaylistRecursive(settings.path, allowedExtensions, &playlist);
    }
    else
    {
        char *token = strtok(search, delimiter);

        while (token != NULL)
        {
            char buf[MAXPATHLEN] = {0};
            if (strncmp(token, "song", 4) == 0)
            {
                memmove(token, token + 4, strlen(token + 4) + 1);
                searchType = FileOnly;
            }
            trim(token);
            if (walker(settings.path, token, buf, allowedExtensions, searchType) == 0)
            {
                if (strcmp(argv[1], "list") == 0)
                {
                    readM3UFile(buf, &playlist);
                }
                else
                {
                    buildPlaylistRecursive(buf, allowedExtensions, &partialPlaylist);
                    joinPlaylist(&playlist, &partialPlaylist);
                }
            }

            token = strtok(NULL, delimiter);
        }
    }
    if (numDirs > 1)
        shuffle = true;
    if (shuffle)
        shufflePlaylist(&playlist);

    if (playlist.head == NULL)
        puts("Music not found");

    return 0;
}

void *getDurationsThread(void *arg)
{
    PlayList *playList = (PlayList *)arg;

    if (playList == NULL)
        return NULL;

    Node *currentNode = playList->head;
    double totalDuration = 0.0;

    for (int i = 0; i < playList->count; i++)
    {
        if (stopPlaylistDurationThread)
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
        c_strcpy(musicFilepath, sizeof(musicFilepath), currentNode->song.filePath);
        double duration = ((currentNode->song.duration > 0.0) ? currentNode->song.duration : getDuration(musicFilepath));
        if (duration > 0.0)
        {
            currentNode->song.duration = duration;
            totalDuration += duration;
            playList->totalDuration += duration;
        }
        currentNode = getListNext(currentNode);
        
        c_sleep(100);
    }
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

int calculatePlayListDuration(PlayList *playlist)
{
    if (playlist->count > MAX_COUNT_PLAYLIST_SONGS)
        return 0;

    startPlayListDurationCount();

    if (playlist->totalDuration > 0.0)
        return 0;

    pthread_t thread;
    int threadCreationResult;
    threadCreationResult = pthread_create(&thread, NULL, getDurationsThread, (void *)playlist);
    if (threadCreationResult != 0)
    {
        return 1;
    }
    return 0;
}

void stopPlayListDurationCount()
{
    stopPlaylistDurationThread = 1;
}

void startPlayListDurationCount()
{
    stopPlaylistDurationThread = 0;
}

void readM3UFile(const char *filename, PlayList *playlist)
{
    FILE *file = fopen(filename, "r");
    char directory[MAXPATHLEN];

    if (file == NULL)
    {
        return;
    }

    getDirectoryFromPath(filename, directory);
    char line[MAXPATHLEN];
    while (fgets(line, sizeof(line), file))
    {
        size_t len = strcspn(line, "\r\n");
        line[len] = '\0';

        size_t start = 0;
        while (isspace(line[start]))
        {
            start++;
        }
        size_t end = strlen(line);
        while (end > start && isspace(line[end - 1]))
        {
            end--;
        }
        line[end] = '\0';

        if (line[0] != '#' && line[0] != '\0')
        {
            char songPath[MAXPATHLEN];
            memset(songPath, '\0', sizeof(songPath));

            if (strchr(line, '/') == NULL && strchr(line, '\\') == NULL)
                strcat(songPath, directory);

            strcat(songPath, line);

            Node *newNode = (Node *)malloc(sizeof(Node));
            newNode->song.filePath = strdup(songPath);
            newNode->song.duration = 0.0;
            newNode->next = NULL;
            newNode->prev = NULL;

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
        }
    }
    fclose(file);
}

void writeM3UFile(const char *filename, PlayList *playlist)
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

void loadMainPlaylist(const char *directory)
{
    char playlistPath[MAXPATHLEN];
    c_strcpy(playlistPath, sizeof(playlistPath), directory);
    if (playlistPath[strlen(playlistPath) - 1] != '/')
        strcat(playlistPath, "/");
    strcat(playlistPath, mainPlaylistName);
    mainPlaylist = malloc(sizeof(PlayList));
    if (mainPlaylist == NULL)
    {
        printf("Failed to allocate memory for mainPlaylist.\n");
        exit(0);
    }
    mainPlaylist->count = 0;
    mainPlaylist->totalDuration = 0;
    mainPlaylist->head = NULL;
    mainPlaylist->tail = NULL;
    readM3UFile(playlistPath, mainPlaylist);
}

void saveMainPlaylist(const char *directory, bool isPlayingMain)
{
    char playlistPath[MAXPATHLEN];
    c_strcpy(playlistPath, sizeof(playlistPath), directory);
    if (playlistPath[strlen(playlistPath) - 1] != '/')
        strcat(playlistPath, "/");
    strcat(playlistPath, mainPlaylistName);
    if (isPlayingMain)
        writeM3UFile(playlistPath, &playlist);
    else
        writeM3UFile(playlistPath, mainPlaylist);
}

void savePlaylist()
{
    char playlistPath[MAXPATHLEN];
    c_strcpy(playlistPath, sizeof(playlistPath), settings.path);
    if (playlistPath[strlen(playlistPath) - 1] != '/')
        strcat(playlistPath, "/");
    strcat(playlistPath, playlistName);
    writeM3UFile(playlistPath, &playlist);
}

Node* deepCopyNode(Node* originalNode) {
    if (originalNode == NULL) {
        return NULL;
    }

    Node* newNode = malloc(sizeof(Node));
    if (newNode == NULL) {
        return NULL;
    }

    newNode->song.filePath = strdup(originalNode->song.filePath);
    newNode->song.duration = originalNode->song.duration;
    newNode->prev = NULL;
    newNode->next = deepCopyNode(originalNode->next);

    if (newNode->next != NULL) {
        newNode->next->prev = newNode;
    }

    return newNode;
}

PlayList deepCopyPlayList(PlayList *originalList)
{
    if (originalList == NULL)
    {
        PlayList newList = {NULL, NULL, 0, 0.0};
        return newList;
    }

    PlayList newList;
    newList.head = deepCopyNode(originalList->head);
    newList.tail = deepCopyNode(originalList->tail);
    newList.count = originalList->count;
    newList.totalDuration = originalList->totalDuration;

    return newList;
}

Node* findSongInPlaylist(Node* currentSong, PlayList* playlist) {
    Node* currentNode = playlist->head;

    while (currentNode != NULL) {
        if (strcmp(currentNode->song.filePath, currentSong->song.filePath) == 0) {
            return currentNode;
        }

        currentNode = currentNode->next;
    }
    return NULL;
}