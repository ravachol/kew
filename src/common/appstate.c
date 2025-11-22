/**
 * @file appstate.c
 * @brief Provides globally accessible state structs, getters and setters.
 *
 */

#include "common/appstate.h"
#include "utils/utils.h"

AppState app_state;

FileSystemEntry *library = NULL;

PlaybackState playback_state;

AudioData audio_data;

AppSettings settings;

// The (sometimes shuffled) sequence of songs that will be played
PlayList playlist = {NULL, NULL, 0, PTHREAD_MUTEX_INITIALIZER};

// The playlist unshuffled as it appears in playlist view
PlayList *unshuffled_playlist = NULL;

// The playlist from kew favorites .m3u
PlayList *favorites_playlist = NULL;

static const char LIBRARY_FILE[] = "library.dat";

double pause_seconds = 0.0;

double total_pause_seconds = 0.0;

Node *next_song = NULL;

Node *try_next_song = NULL;

Node *song_to_start_from = NULL;

ProgressBar progress_bar;

void free_playlists(void)
{
        empty_playlist(&playlist);
        pthread_mutex_destroy(&playlist.mutex);

        PlayList *unshuffled = get_unshuffled_playlist();
        PlayList *favorites = get_favorites_playlist();

        if (unshuffled != NULL) {
                empty_playlist(unshuffled);
                pthread_mutex_destroy(&unshuffled->mutex);
                free(unshuffled);
                unshuffled_playlist = NULL;
        }

        if (favorites != NULL) {
                empty_playlist(favorites);
                pthread_mutex_destroy(&favorites->mutex);
                free(favorites);
                favorites_playlist = NULL;
        }
}

void create_playlist(PlayList **playlist)
{
        if (*playlist == NULL) {
                *playlist = malloc(sizeof(PlayList));
                if (*playlist == NULL) {
                        return;
                }

                (*playlist)->count = 0;
                (*playlist)->head = NULL;
                (*playlist)->tail = NULL;
                pthread_mutex_init(&(*playlist)->mutex, NULL);
        }
}

// --- Getters ---

AudioData *get_audio_data(void)
{
        return &audio_data;
}

AppState *get_app_state()
{
        return &app_state;
}

AppSettings *get_app_settings()
{
        return &settings;
}

FileSystemEntry *get_library()
{
        return library;
}

PlaybackState *get_playback_state()
{
        return &playback_state;
}

char *get_library_file_path(void)
{
        return get_file_path(LIBRARY_FILE);
}

double get_pause_seconds(void)
{
        return pause_seconds;
}

double get_total_pause_seconds(void)
{
        return total_pause_seconds;
}

Node *get_next_song(void)
{
        return next_song;
}

Node *get_song_to_start_from(void)
{
        return song_to_start_from;
}

Node *get_try_next_song(void)
{
        return try_next_song;
}

ProgressBar *get_progress_bar(void)
{
        return &progress_bar;
}

PlayList *get_playlist(void)
{
        return &playlist;
}

PlayList *get_unshuffled_playlist(void)
{
        return unshuffled_playlist;
}

PlayList *get_favorites_playlist(void)
{
        return favorites_playlist;
}

// --- Setters ---

void set_audio_data(AudioData *ad)
{
        if (ad)
                audio_data = *ad;
}

void set_library(FileSystemEntry *root)
{
        library = root;
}

void set_unshuffled_playlist(PlayList *pl)
{
        if (pl == unshuffled_playlist) {
                return;
        }

        if (unshuffled_playlist != NULL) {
                empty_playlist(unshuffled_playlist);
                pthread_mutex_destroy(&unshuffled_playlist->mutex);
                free(unshuffled_playlist);
        }
        unshuffled_playlist = pl;
}

void set_favorites_playlist(PlayList *pl)
{
        if (pl == favorites_playlist) {
                return;
        }

        if (favorites_playlist != NULL) {
                empty_playlist(favorites_playlist);
                pthread_mutex_destroy(&favorites_playlist->mutex);
                free(favorites_playlist);
        }
        favorites_playlist = pl;
}

void set_pause_seconds(double seconds)
{
        pause_seconds = seconds;
}

void set_total_pause_seconds(double seconds)
{
        total_pause_seconds = seconds;
}

void set_next_song(Node *node)
{
        next_song = node;
}

void set_song_to_start_from(Node *node)
{
        song_to_start_from = node;
}

void set_try_next_song(Node *node)
{
        try_next_song = node;
}
