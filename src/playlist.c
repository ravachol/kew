#define _XOPEN_SOURCE 700
#define __USE_XOPEN_EXTENDED 1

#include "playlist.h"

/*

playlist.c

Playlist related functions.

*/
#define MAX_SEARCH_SIZE 256

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

const char PLAYLIST_EXTENSIONS[] = "(m3u8?)$";
const char mainPlaylistName[] = "kew.m3u";
const char lastUsedPlaylistName[] = "lastPlaylist.m3u";

// The playlist unshuffled as it appears in playlist view
PlayList *originalPlaylist = NULL;

// The (sometimes shuffled) sequence of songs that will be played
PlayList playlist = {NULL, NULL, 0, PTHREAD_MUTEX_INITIALIZER};

// The playlist from kew.m3u
PlayList *specialPlaylist = NULL;

char search[MAX_SEARCH_SIZE];
char playlistName[MAX_SEARCH_SIZE];
bool shuffle = false;
int numDirs = 0;
volatile int stopPlaylistDurationThread = 0;
Node *currentSong = NULL;
int nodeIdCounter = 0;

Node *getListNext(Node *node)
{
        return (node == NULL) ? NULL : node->next;
}

Node *getListPrev(Node *node)
{
        return (node == NULL) ? NULL : node->prev;
}

void addToList(PlayList *list, Node *newNode)
{
        if (list->count >= MAX_FILES)
                return;

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

void moveUpList(PlayList *list, Node *node)
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

void moveDownList(PlayList *list, Node *node)
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

Node *deleteFromList(PlayList *list, Node *node)
{
        if (list->head == NULL || node == NULL)
                return NULL;

        if (list->head == node)
        {
                list->head = node->next;
                if (list->head == NULL)
                {
                        list->tail = NULL;
                }
        }

        if (node == list->tail)
                list->tail = node->prev;

        if (node->prev != NULL)
                node->prev->next = node->next;
        if (node->next != NULL)
                node->next->prev = node->prev;

        if (node->song.filePath != NULL)
                free(node->song.filePath);

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
                free(current->song.filePath);
                free(current);
                current = next;
        }

        // Reset the playlist
        list->head = NULL;
        list->tail = NULL;
        list->count = 0;
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
                nodes[j]->next = (j < playlist->count - 1) ? nodes[j + 1] : NULL;
                nodes[j]->prev = (j > 0) ? nodes[j - 1] : NULL;
        }
        free(nodes);
}

void insertAsFirst(Node *currentSong, PlayList *playlist)
{
        if (currentSong == NULL || playlist == NULL)
        {
                return;
        }

        if (playlist->head == NULL)
        {
                currentSong->next = NULL;
                currentSong->prev = NULL;
                playlist->head = currentSong;
                playlist->tail = currentSong;
        }
        else
        {
                if (currentSong != playlist->head)
                {
                        if (currentSong->next != NULL)
                        {
                                currentSong->next->prev = currentSong->prev;
                        }
                        else
                        {
                                playlist->tail = currentSong->prev;
                        }

                        if (currentSong->prev != NULL)
                        {
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
        if (song != NULL && playlist->count > 1)
        {
                insertAsFirst(song, playlist);
        }
}

void createNode(Node **node, const char *directoryPath, int id)
{
        SongInfo song;
        song.filePath = strdup(directoryPath);
        song.duration = 0.0;

        *node = (Node *)malloc(sizeof(Node));
        if (*node == NULL)
        {
                printf("Failed to allocate memory.");
                free(song.filePath);
                exit(0);
                return;
        }

        (*node)->song = song;
        (*node)->next = NULL;
        (*node)->prev = NULL;
        (*node)->id = id;
}

void buildPlaylistRecursive(const char *directoryPath, const char *allowedExtensions, PlayList *playlist)
{
        int res = isDirectory(directoryPath);
        if (res != 1 && res != -1 && directoryPath != NULL)
        {
                Node *node = NULL;
                createNode(&node, directoryPath, nodeIdCounter++);
                addToList(playlist, node);
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

        char exto[100];
        struct dirent **entries;
        int numEntries = scandir(directoryPath, &entries, NULL, compareLibEntries);

        if (numEntries < 0)
        {
                printf("Failed to scan directory: %s\n", directoryPath);
                return;
        }

        for (int i = 0; i < numEntries && playlist->count < MAX_FILES; i++)
        {
                struct dirent *entry = entries[i];

                if (entry->d_name[0] == '.' || strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
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
                                snprintf(filePath, sizeof(filePath), "%s/%s", directoryPath, entry->d_name);

                                Node *node = NULL;
                                createNode(&node, filePath, nodeIdCounter++);
                                addToList(playlist, node);
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

void makePlaylistName(const char *search, int maxSize)
{
        playlistName[0] = '\0';

        snprintf(playlistName, maxSize, "%s", search);

        snprintf(playlistName + strnlen(playlistName, maxSize), maxSize - strnlen(playlistName, maxSize), ".m3u");

        for (int i = 0; playlistName[i] != '\0'; i++)
        {
                if (playlistName[i] == ':')
                {
                        playlistName[i] = '-';
                }
        }
}

void readM3UFile(const char *filename, PlayList *playlist)
{
        GError *error = NULL;
        gchar *contents;
        gchar **lines;

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
                                songPath = g_build_filename(directory, trimmed_line, NULL);
                        }

                        Node *newNode = NULL;
                        createNode(&newNode, songPath, nodeIdCounter++);

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

int makePlaylist(int argc, char *argv[], bool exactSearch, const char *path)
{
        enum SearchType searchType = SearchAny;
        int searchTypeIndex = 1;

        const char *delimiter = ":";
        PlayList partialPlaylist = {NULL, NULL, 0, PTHREAD_MUTEX_INITIALIZER};

        const char *allowedExtensions = AUDIO_EXTENSIONS;

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
                size_t len = strnlen(search, MAX_SEARCH_SIZE);
                snprintf(search + len, MAX_SEARCH_SIZE - len, " %s", argv[i]);
        }
        makePlaylistName(search, MAX_SEARCH_SIZE);

        if (strstr(search, delimiter))
        {
                shuffle = true;
        }

        if (searchType == ReturnAllSongs)
        {
                pthread_mutex_lock(&(playlist.mutex));

                buildPlaylistRecursive(path, allowedExtensions, &playlist);

                pthread_mutex_unlock(&(playlist.mutex));
        }
        else
        {
                char *token = strtok(search, delimiter);

                while (token != NULL)
                {
                        char buf[MAXPATHLEN] = {0};
                        if (strncmp(token, "song", 4) == 0)
                        {
                                memmove(token, token + 4, strnlen(token + 4, MAXPATHLEN) + 1);
                                searchType = FileOnly;
                        }
                        trim(token, MAXPATHLEN);
                        char *searching = g_utf8_casefold(token, -1);

                        if (walker(path, searching, buf, allowedExtensions, searchType, exactSearch) == 0)
                        {
                                if (strcmp(argv[1], "list") == 0)
                                {
                                        readM3UFile(buf, &playlist);
                                }
                                else
                                {
                                        pthread_mutex_lock(&(playlist.mutex));

                                        buildPlaylistRecursive(buf, allowedExtensions, &partialPlaylist);
                                        joinPlaylist(&playlist, &partialPlaylist);

                                        pthread_mutex_unlock(&(playlist.mutex));
                                }
                        }
                        free(searching);
                        token = strtok(NULL, delimiter);
                }
        }
        if (numDirs > 1)
                shuffle = true;
        if (shuffle)
                shufflePlaylist(&playlist);

        if (playlist.head == NULL)
                printf("Music not found\n");

        return 0;
}

void generateM3UFilename(const char *basePath, const char *filePath, char *m3uFilename, size_t size)
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
                        snprintf(m3uFilename, size, "%s%s.m3u", basePath, baseName);
                }
                else
                {
                        snprintf(m3uFilename, size, "%s/%s.m3u", basePath, baseName);
                }
        }
        else
        {
                // Copy the base name up to the dot and append ".m3u"
                size_t baseNameLen = dot - baseName;
                if (basePath[strnlen(basePath, MAXPATHLEN) - 1] == '/')
                {
                        snprintf(m3uFilename, size, "%s%.*s.m3u", basePath, (int)baseNameLen, baseName);
                }
                else
                {
                        snprintf(m3uFilename, size, "%s/%.*s.m3u", basePath, (int)baseNameLen, baseName);
                }
        }
}

void writeM3UFile(const char *filename, const PlayList *playlist)
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

void loadPlaylist(const char *directory, const char *playlistName, PlayList *playlist)
{
        char playlistPath[MAXPATHLEN];

        if (directory[strnlen(directory, MAXPATHLEN) - 1] == '/')
        {
                snprintf(playlistPath, sizeof(playlistPath), "%s%s", directory, playlistName);
        }
        else
        {
                snprintf(playlistPath, sizeof(playlistPath), "%s/%s", directory, playlistName);
        }

        if (playlist == NULL)
        {
                printf("Failed to allocate memory for special playlist.\n");
                exit(0);
        }
        playlist->count = 0;
        playlist->head = NULL;
        playlist->tail = NULL;
        readM3UFile(playlistPath, playlist);
}

void loadSpecialPlaylist(const char *directory)
{
        specialPlaylist = malloc(sizeof(PlayList));
        loadPlaylist(directory, mainPlaylistName, specialPlaylist);
}

void loadLastUsedPlaylist(void)
{
        loadPlaylist(getConfigPath(), lastUsedPlaylistName, &playlist);

        if (originalPlaylist == NULL)
        {

                originalPlaylist = malloc(sizeof(PlayList));
                *originalPlaylist = deepCopyPlayList(&playlist);
        }
}

void saveNamedPlaylist(const char *directory, const char *playlistName, const PlayList *playlist)
{
    if (directory == NULL)
    {
        return;
    }

    char playlistPath[MAXPATHLEN];

    int length = snprintf(playlistPath, sizeof(playlistPath), "%s", directory);

    if (length < 0 || length >= (int)sizeof(playlistPath) || playlistPath[0] == '\0')
    {
        return;
    }

    if (playlistPath[length - 1] != '/')
    {
        snprintf(playlistPath + length, sizeof(playlistPath) - length, "/%s", playlistName);
    }
    else
    {
        snprintf(playlistPath + length, sizeof(playlistPath) - length, "%s", playlistName);
    }

    if (playlist != NULL)
    {
        writeM3UFile(playlistPath, playlist);
    }
}

void saveSpecialPlaylist(const char *directory)
{
        if (specialPlaylist != NULL && specialPlaylist->count > 0)
        {
                saveNamedPlaylist(directory, mainPlaylistName, specialPlaylist);
        }
}

void saveLastUsedPlaylist(void)
{
        saveNamedPlaylist(getConfigPath(), lastUsedPlaylistName, originalPlaylist);
}

void savePlaylist(const char *path, const PlayList *playlist)
{
        if (path == NULL)
        {
                return;
        }

        if (playlist->head == NULL || playlist->head->song.filePath == NULL)
                return;

        writeM3UFile(path, playlist);
}

void exportCurrentPlaylist(const char *path)
{
        char m3uFilename[MAXPATHLEN];

        generateM3UFilename(path, playlist.head->song.filePath, m3uFilename, sizeof(m3uFilename));

        savePlaylist(m3uFilename, &playlist);
}

Node *deepCopyNode(Node *originalNode)
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
        newNode->next = deepCopyNode(originalNode->next);

        if (newNode->next != NULL)
        {
                newNode->next->prev = newNode;
        }

        return newNode;
}

Node *findTail(Node *head)
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

PlayList deepCopyPlayList(PlayList *originalList)
{
        PlayList newList = {NULL, NULL, 0, PTHREAD_MUTEX_INITIALIZER};

        deepCopyPlayListOntoList(originalList, &newList);
        return newList;
}

void deepCopyPlayListOntoList(PlayList *originalList, PlayList *newList)
{
        if (originalList == NULL)
        {
                return;
        }

        newList->head = deepCopyNode(originalList->head);
        newList->tail = findTail(newList->head);
        newList->count = originalList->count;
}

Node *findPathInPlaylist(const char *path, PlayList *playlist)
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

Node *findLastPathInPlaylist(const char *path, PlayList *playlist)
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

int findNodeInList(PlayList *list, int id, Node **foundNode)
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

void addSongToPlayList(PlayList *list, const char *filePath, int playlistMax)
{
        if (list->count >= playlistMax)
                return;

        Node *newNode = NULL;
        createNode(&newNode, filePath, list->count);
        addToList(list, newNode);
}

void traverseFileSystemEntry(FileSystemEntry *entry, PlayList *list, int playlistMax)
{
        if (entry == NULL || list->count >= playlistMax)
                return;

        if (entry->isDirectory == 0)
        {
                addSongToPlayList(list, entry->fullPath, playlistMax);
        }

        if (entry->isDirectory == 1 && entry->children != NULL)
        {
                traverseFileSystemEntry(entry->children, list, playlistMax);
        }

        if (entry->next != NULL)
        {
                traverseFileSystemEntry(entry->next, list, playlistMax);
        }
}

void createPlayListFromFileSystemEntry(FileSystemEntry *root, PlayList *list, int playlistMax)
{
        traverseFileSystemEntry(root, list, playlistMax);
}

int isMusicFile(const char *filename)
{
        const char *extensions[] = {".m4a", ".aac", ".mp3", ".ogg", ".flac", ".wav", ".opus", ".webm"};

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

int containsMusicFiles(FileSystemEntry *entry)
{
        if (entry == NULL)
                return 0;

        FileSystemEntry *child = entry->children;

        while (child != NULL)
        {
                if (!child->isDirectory && isMusicFile(child->name))
                {
                        return 1;
                }
                child = child->next;
        }

        return 0;
}

void addAlbumToPlayList(PlayList *list, FileSystemEntry *album, int playlistMax)
{
        FileSystemEntry *entry = album->children;

        while (entry != NULL && list->count < playlistMax)
        {
                if (!entry->isDirectory && isMusicFile(entry->name))
                {
                        addSongToPlayList(list, entry->fullPath, playlistMax);
                }
                entry = entry->next;
        }
}

void addAlbumsToPlayList(FileSystemEntry *entry, PlayList *list, int playlistMax)
{
        if (entry == NULL || list->count >= playlistMax)
                return;

        if (entry->isDirectory && containsMusicFiles(entry))
        {
                addAlbumToPlayList(list, entry, playlistMax);
        }

        if (entry->isDirectory && entry->children != NULL)
        {
                addAlbumsToPlayList(entry->children, list, playlistMax);
        }

        if (entry->next != NULL)
        {
                addAlbumsToPlayList(entry->next, list, playlistMax);
        }
}

void shuffleEntries(FileSystemEntry **array, size_t n)
{
    if (n > 1)
    {
        for (size_t i = 0; i < n - 1; i++)
        {
            size_t j = getRandomNumber(i, n - 1);

            // Swap entries at i and j
            FileSystemEntry *tmp = array[i];
            array[i] = array[j];
            array[j] = tmp;
        }
    }
}

void collectAlbums(FileSystemEntry *entry, FileSystemEntry **albums, size_t *count)
{
        if (entry == NULL)
                return;

        if (entry->isDirectory && containsMusicFiles(entry))
        {
                albums[*count] = entry;
                (*count)++;
        }

        if (entry->isDirectory && entry->children != NULL)
        {
                collectAlbums(entry->children, albums, count);
        }

        if (entry->next != NULL)
        {
                collectAlbums(entry->next, albums, count);
        }
}

void addShuffledAlbumsToPlayList(FileSystemEntry *root, PlayList *list, int playlistMax)
{
        size_t maxAlbums = 2000;
        FileSystemEntry *albums[maxAlbums];
        size_t albumCount = 0;

        collectAlbums(root, albums, &albumCount);

        shuffleEntries(albums, albumCount);

        for (size_t i = 0; i < albumCount && list->count < playlistMax; i++)
        {
                addAlbumToPlayList(list, albums[i], playlistMax);
        }
}
