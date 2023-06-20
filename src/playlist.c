#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <regex.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "playlist.h"
#include "dir.h"
#include "stringfunc.h"
#include "settings.h"

#define MAX_SEARCH_SIZE 256
#define MAX_FILES 10000

const char ALLOWED_EXTENSIONS[] = "\\.(m4a|mp3|ogg|flac|wav|aac|wma|raw|mp4a|mp4)$";
const char PLAYLIST_EXTENSIONS[] = "\\.(m3u)$";
const char mainPlaylistName[] = "cue.m3u";
PlayList playlist = {NULL, NULL};
PlayList *mainPlaylist = NULL;

char search[MAX_SEARCH_SIZE];
char playlistName[MAX_SEARCH_SIZE];
bool shuffle = false;

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

Node* deleteFromList(PlayList *list, Node *node)
{
    if (list->head == NULL || node == NULL)
        return NULL;

    // Update the head and tail pointers if the node to delete is at the ends
    if (node == list->head)
        list->head = node->next;
    if (node == list->tail)
        list->tail = node->prev;

    // Adjust the next and prev pointers of neighboring nodes
    if (node->prev != NULL)
        node->prev->next = node->next;
    if (node->next != NULL)
        node->next->prev = node->prev;

    // Free the memory allocated for the node
    Node* nextNode = node->next;

    free(node);
    list->count--;
    return nextNode;
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
    for (int j = playlist->count - 1; j >= 1; --j)
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
            int songCount = playlist->count;
            buildPlaylistRecursive(filePath, allowedExtensions, playlist);
            if (playlist->count > songCount)
                shuffle = true;
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

void makePlaylistName(const char *search)
{
    strcat(playlistName, strdup(search));
    strcat(playlistName, ".m3u");   
    int i = 0;
    while(playlistName[i]!='\0')
    {
        if(playlistName[i]==':')
        {
            playlistName[i]='-';
        }
        i++;
    }
}

int makePlaylist(int argc, char *argv[])
{
    enum SearchType searchType = SearchAny;
    int searchTypeIndex = 1;

    const char *delimiter = ":";
    PlayList partialPlaylist = {NULL, NULL};

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

    search[0] = '\0'; // Initialize the search string with a null terminator

    for (int i = start -1; i < argc; i++)
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
            if (walker(settings.path, token, buf, allowedExtensions, searchType) == 0)
            {
                if (strcmp(argv[1], "list") == 0)
                {
                    readM3UFile(buf, &playlist);
                }
                else {
                    buildPlaylistRecursive(buf, allowedExtensions, &partialPlaylist);
                    joinPlaylist(&playlist, &partialPlaylist);
                }
            }

            token = strtok(NULL, delimiter);
        }
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

int calculatePlayListDuration(PlayList *playlist)
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

void readM3UFile(const char* filename, PlayList* playlist) {
    FILE* file = fopen(filename, "r");
    char directory[MAXPATHLEN];

    if (file == NULL) {
        //printf("Failed to open the M3U file.\n");
        return;
    }

    getDirectoryFromPath(filename, directory);
    char line[MAXPATHLEN];
    while (fgets(line, sizeof(line), file)) {
        // Remove trailing newline character
        size_t len = strcspn(line, "\r\n");
        line[len] = '\0';

        // Remove leading and trailing whitespace characters
        size_t start = 0;
        while (isspace(line[start])) {
            start++;
        }
        size_t end = strlen(line);
        while (end > start && isspace(line[end - 1])) {
            end--;
        }
        line[end] = '\0';

        // Check if the line is a valid file path
        if (line[0] != '#' && line[0] != '\0') {
            char songPath[MAXPATHLEN];
            memset(songPath, '\0', sizeof(songPath));
            // Check if line is not a complete file path
            if (strchr(line, '/') == NULL && strchr(line, '\\') == NULL)
                strcat(songPath, directory);
           
            strcat(songPath, line);
            // Create a new node for the playlist
            Node* newNode = (Node*)malloc(sizeof(Node));
            newNode->song.filePath = strdup(songPath);  // Duplicate the string for filePath
            newNode->song.duration = 0.0;
            newNode->next = NULL;
            newNode->prev = NULL;

            // Add the node to the playlist
            if (playlist->head == NULL) {
                playlist->head = newNode;
                playlist->tail = newNode;
            } else {
                playlist->tail->next = newNode;
                newNode->prev = playlist->tail;
                playlist->tail = newNode;
            }

            playlist->count++;
        }
    }
    fclose(file);
}

void writeM3UFile(const char* filename, PlayList* playlist) {
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        printf("Failed to open the M3U file for writing.\n");
        return;
    }

    Node* currentNode = playlist->head;
    while (currentNode != NULL) {
        fprintf(file, "%s\n", currentNode->song.filePath);
        currentNode = currentNode->next;
    }
    fclose(file);
}

void loadMainPlaylist(const char *directory)
{
    char playlistPath[MAXPATHLEN];
    strcpy(playlistPath, directory);
    if (playlistPath[strlen(playlistPath) - 1] != '/')
        strcat(playlistPath, "/");
    strcat(playlistPath, mainPlaylistName);
    mainPlaylist = malloc(sizeof(PlayList));
    if (mainPlaylist == NULL) {
        printf("Failed to allocate memory for mainPlaylist.\n");
        // Handle error condition
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
    strcpy(playlistPath, directory);
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
    strcpy(playlistPath, settings.path);
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
    newNode->song.filePath = strdup(originalNode->song.filePath);
    newNode->song.duration = originalNode->song.duration;
    newNode->prev = NULL;  // Initialize the previous node pointer to NULL
    newNode->next = deepCopyNode(originalNode->next);  // Recursively deep copy the next node

    if (newNode->next != NULL) {
        newNode->next->prev = newNode;  // Update the previous pointer of the next node
    }

    return newNode;
}

PlayList deepCopyPlayList(PlayList* originalList) {
    if (originalList == NULL) {
        // Return an empty playlist if the original list is NULL
        PlayList newList = { NULL, NULL, 0, 0.0 };
        return newList;
    }

    PlayList newList;
    newList.head = deepCopyNode(originalList->head);
    newList.tail = deepCopyNode(originalList->tail);
    newList.count = originalList->count;
    newList.totalDuration = originalList->totalDuration;

    return newList;
}
