#include <stdio.h>
#include <stdlib.h>

const int MAX_FILES = 256;

typedef struct 
{
    char* filePath;    
} SongInfo;

typedef struct Node 
{
    SongInfo song;
    struct Node* next;
    struct Node* prev;
} Node;

typedef struct {
    Node* head;
    Node* tail;
    int count;
} PlayList;

Node* getListNext(PlayList* list, Node* node) 
{
    if (node == NULL) {
        return NULL;
    }
    return node->next;
}

Node* getListPrev(PlayList* list, Node* node) 
{
    if (node == NULL) {
        return NULL;
    }
    return node->prev;
}

void addToList(PlayList* list, SongInfo song) 
{  
    if (list->count >= MAX_FILES)
      return;
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->song = song;
    newNode->next = NULL;
    list->count++;

    if (list->head == NULL) {
        // The list is empty
        newNode->prev = NULL;
        list->head = newNode;
        list->tail = newNode;
    } else {
        // Append the new node at the end of the list
        newNode->prev = list->tail;
        list->tail->next = newNode;
        list->tail = newNode;
    }
}

int compare(const struct dirent **a, const struct dirent **b) 
{
    const char *nameA = (*a)->d_name;
    const char *nameB = (*b)->d_name;

    if (nameA[0] == '_' && nameB[0] != '_') {
        return -1;  // Directory A starts with underscore, so it should come first
    } else if (nameA[0] != '_' && nameB[0] == '_') {
        return 1;   // Directory B starts with underscore, so it should come first
    }

    return strcmp(nameA, nameB);  // Lexicographic comparison for other cases
}

void buildPlaylistRecursive(char* directoryPath, const char* allowedExtensions, PlayList* playlist) 
{
    if (!isDirectory(directoryPath) && directoryPath != NULL) {
        SongInfo song;
        song.filePath = strdup(directoryPath);
        addToList(playlist, song);
        return;
    }

    DIR* dir = opendir(directoryPath);
    if (dir == NULL) {
        printf("Failed to open directory: %s\n", directoryPath);
        return;
    }

    regex_t regex;
    int ret = regcomp(&regex, allowedExtensions, REG_EXTENDED);
    if (ret != 0) {
        printf("Failed to compile regular expression\n");
        closedir(dir);
        return;
    }

    char exto[6]; // +1 for null-terminator

    struct dirent **entries;
    int numEntries = scandir(directoryPath, &entries, NULL, compare);
    if (numEntries < 0) {
        printf("Failed to scan directory: %s\n", directoryPath);
        return;
    }
    
    for (int i = 0; i < numEntries && playlist->count < MAX_FILES; i++) {
        struct dirent* entry = entries[i];

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;  // Skip current and parent directory entries
        }

       char filePath[FILENAME_MAX];
       snprintf(filePath, sizeof(filePath), "%s/%s", directoryPath, entry->d_name);

        if (isDirectory(filePath)) {
            // Entry is a directory, recursively process it
            buildPlaylistRecursive(filePath, allowedExtensions, playlist);
        } else {
            // Entry is a regular file, check if it matches the desired file format
            extractExtension(entry->d_name, sizeof(exto) - 1, exto);
            if (match_regex(&regex, exto) == 0) {
                SongInfo song;
                snprintf(filePath, sizeof(filePath), "%s/%s", directoryPath, entry->d_name);
                song.filePath = strdup(filePath); // Allocate memory and copy the file path
                addToList(playlist, song);
            }
        }
    }

    closedir(dir);
    regfree(&regex);
}

int playDirectory(const char* directoryPath, const char* allowedExtensions, PlayList* playlist) 
{
    DIR* dir = opendir(directoryPath);
    if (dir == NULL) {
        printf("Failed to open directory: %s\n", directoryPath);
        return -1;
    }

    regex_t regex;
    int ret = regcomp(&regex, allowedExtensions, REG_EXTENDED);
    if (ret != 0) {
        return -1;
    }    
    char ext[6]; // +1 for null-terminator, should be able to handle .flac
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        extractExtension(entry->d_name, sizeof(ext) - 1, ext);
        if (match_regex(&regex, ext) == 0) {    
            char filePath[FILENAME_MAX];
            snprintf(filePath, sizeof(filePath), "%s/%s", directoryPath, entry->d_name);
            SongInfo song;
            song.filePath = strdup(filePath);  // Allocate memory and copy the file path
            addToList(playlist, song);
        }
    }
    closedir(dir);

    return 0;
}