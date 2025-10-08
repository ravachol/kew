#include "appstate.h"

AppState appState;

FileSystemEntry *library = NULL;

AudioData audioData;

AppSettings settings;

// The (sometimes shuffled) sequence of songs that will be played
PlayList playlist = {NULL, NULL, 0, PTHREAD_MUTEX_INITIALIZER};

// The playlist unshuffled as it appears in playlist view
PlayList *unshuffledPlaylist = NULL;

// The playlist from kew favorites .m3u
PlayList *favoritesPlaylist = NULL;

double pauseSeconds = 0.0;

double totalPauseSeconds = 0.0;

Node *nextSong = NULL;

Node *tryNextSong = NULL;

Node *songToStartFrom = NULL;

ProgressBar progressBar;

void freePlaylists(void)
{
        deletePlaylist(&playlist);
        deletePlaylist(unshuffledPlaylist);
        deletePlaylist(favoritesPlaylist);
        free(favoritesPlaylist);
        free(unshuffledPlaylist);
}

void createPlaylist(PlayList **playlist)
{
    if (*playlist == NULL)
    {
        *playlist = malloc(sizeof(PlayList));
        if (*playlist == NULL)
        {
                return;
        }

        (*playlist)->count = 0;
        (*playlist)->head = NULL;
        (*playlist)->tail = NULL;
        pthread_mutex_init(&(*playlist)->mutex, NULL);
    }
}

// --- Getters ---

AudioData *getAudioData(void)
{
        return &audioData;
}

AppState *getAppState()
{
        return &appState;
}

AppSettings *getAppSettings()
{
        return &settings;
}

FileSystemEntry *getLibrary()
{
        return library;
}

double getPauseSeconds(void)
{
        return pauseSeconds;
}

double getTotalPauseSeconds(void)
{
        return totalPauseSeconds;
}

Node *getNextSong(void)
{
        return nextSong;
}

Node *getSongToStartFrom(void)
{
        return songToStartFrom;
}

Node *getTryNextSong(void)
{
        return tryNextSong;
}

ProgressBar *getProgressBar(void)
{
        return &progressBar;
}

PlayList *getPlaylist(void)
{
        return &playlist;
}

PlayList *getUnshuffledPlaylist(void)
{
        return unshuffledPlaylist;
}

PlayList *getFavoritesPlaylist(void)
{
        return favoritesPlaylist;
}

// --- Setters ---

void setAudioData(AudioData *ad)
{
        if (ad)
                audioData = *ad;
}

void setLibrary(FileSystemEntry *root)
{
        library = root;
}

void setPlaylist(PlayList *pl)
{
        if (pl)
                playlist = *pl;
}

void setUnshuffledPlaylist(PlayList *pl)
{
        unshuffledPlaylist = pl;
}

void setFavoritesPlaylist(PlayList *pl)
{
        favoritesPlaylist = pl;
}

void setPauseSeconds(double seconds)
{
        pauseSeconds = seconds;
}

void setTotalPauseSeconds(double seconds)
{
        totalPauseSeconds = seconds;
}

void setNextSong(Node *node)
{
        nextSong = node;
}

void setSongToStartFrom(Node *node)
{
        songToStartFrom = node;
}

void setTryNextSong(Node *node)
{
        tryNextSong = node;
}
