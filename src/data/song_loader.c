/**
 * @file song_loader.c
 * @brief Song loading and preparation routines.
 *
 * Responsible for loading song data from file
 */

#include "song_loader.h"

#include "img_func.h"
#include "lyrics.h"

#include "utils/cache.h"
#include "utils/file.h"
#include "utils/utils.h"

#include "stb_image.h"
#include "tagLibWrapper.h"

#include <dirent.h>
#include <gio/gio.h>
#include <glib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_RECURSION_DEPTH 10

static guint track_counter = 0;

void make_file_path(const char *dir_path, char *file_path, size_t file_path_size,
                    const struct dirent *entry)
{
        if (dir_path == NULL || file_path == NULL || entry == NULL ||
            file_path_size == 0)
                return;

        size_t dirLen = strnlen(dir_path, file_path_size);
        size_t nameLen = strnlen(entry->d_name, file_path_size);

        if (dirLen == file_path_size) {
                file_path[0] = '\0';
                return;
        }

        size_t neededSize = dirLen + nameLen + 1; // +1 for '\0'
        if (dir_path[dirLen - 1] != '/') {
                neededSize += 1; // for the added '/'
        }

        if (neededSize > file_path_size) {
                file_path[0] = '\0';
                return;
        }

        // Compose the path safely
        if (dir_path[dirLen - 1] == '/') {
                snprintf(file_path, file_path_size, "%s%s", dir_path,
                         entry->d_name);
        } else {
                snprintf(file_path, file_path_size, "%s/%s", dir_path,
                         entry->d_name);
        }

        // snprintf guarantees null termination if file_path_size > 0
}

char *choose_album_art(const char *dir_path, char **custom_file_name_arr, int size,
                       int depth)
{
        if (!dir_path || !custom_file_name_arr || size <= 0 ||
            depth > MAX_RECURSION_DEPTH) {
                return NULL;
        }

        DIR *directory = opendir(dir_path);
        if (!directory) {
                return NULL;
        }

        struct dirent *entry;
        struct stat file_stat;
        char file_path[PATH_MAX];
        char resolved_path[PATH_MAX];
        char *result = NULL;

        for (int i = 0; i < size && !result; i++) {
                rewinddir(directory);
                while ((entry = readdir(directory)) != NULL) {
                        if (strcmp(entry->d_name, ".") == 0 ||
                            strcmp(entry->d_name, "..") == 0)
                                continue;

                        int written = snprintf(file_path, sizeof(file_path),
                                               "%s/%s", dir_path, entry->d_name);
                        if (written < 0 || written >= (int)sizeof(file_path)) {
                                continue; // path too long
                        }

                        if (realpath(file_path, resolved_path) == NULL) {
                                continue;
                        }

                        if (strncmp(resolved_path, dir_path, strlen(dir_path)) !=
                            0) {
                                continue; // outside allowed directory
                        }

                        if (stat(resolved_path, &file_stat) == 0 &&
                            S_ISREG(file_stat.st_mode)) {
                                if (strcmp(entry->d_name,
                                           custom_file_name_arr[i]) == 0) {
                                        result = strdup(resolved_path);
                                        break;
                                }
                        }
                }
        }

        // Recursive search for directories
        if (!result) {
                rewinddir(directory);
                while ((entry = readdir(directory)) != NULL && !result) {
                        if (strcmp(entry->d_name, ".") == 0 ||
                            strcmp(entry->d_name, "..") == 0)
                                continue;

                        int written = snprintf(file_path, sizeof(file_path),
                                               "%s/%s", dir_path, entry->d_name);
                        if (written < 0 || written >= (int)sizeof(file_path)) {
                                continue;
                        }

                        if (realpath(file_path, resolved_path) == NULL) {
                                continue;
                        }

                        if (strncmp(resolved_path, dir_path, strlen(dir_path)) !=
                            0) {
                                continue;
                        }

                        struct stat link_stat;
                        if (lstat(resolved_path, &link_stat) == 0) {
                                if (S_ISLNK(link_stat.st_mode)) {
                                        continue; // skip symlink
                                }
                                if (S_ISDIR(link_stat.st_mode)) {
                                        result = choose_album_art(
                                            resolved_path, custom_file_name_arr,
                                            size, depth + 1);
                                }
                        }
                }
        }

        closedir(directory);
        return result;
}

char *find_largest_image_file(const char *directory_path, char *largest_image_file,
                              off_t *largest_file_size)
{
        DIR *directory = opendir(directory_path);
        if (directory == NULL) {
                fprintf(stderr, "Failed to open directory: %s\n",
                        directory_path);
                return largest_image_file;
        }

        struct dirent *entry;
        struct stat file_stats;
        char file_path[PATH_MAX];
        char resolved_path[PATH_MAX];

        while ((entry = readdir(directory)) != NULL) {
                // Skip "." and ".."
                if (strcmp(entry->d_name, ".") == 0 ||
                    strcmp(entry->d_name, "..") == 0)
                        continue;

                // Construct file path safely
                int len = snprintf(file_path, sizeof(file_path), "%s/%s",
                                   directory_path, entry->d_name);
                if (len < 0 || len >= (int)sizeof(file_path)) {
                        // Path too long, skip
                        continue;
                }

                // Resolve the real path
                if (realpath(file_path, resolved_path) == NULL) {
                        // Could not resolve, skip
                        continue;
                }

                // Use lstat to avoid following symlinks
                if (lstat(resolved_path, &file_stats) == -1) {
                        continue;
                }

                if (S_ISLNK(file_stats.st_mode)) {
                        // Ignore symlinks
                        continue;
                }

                if (S_ISREG(file_stats.st_mode)) {
                        // Validate extension
                        char *extension = strrchr(entry->d_name, '.');
                        if (extension != NULL &&
                            (strcasecmp(extension, ".jpg") == 0 ||
                             strcasecmp(extension, ".jpeg") == 0 ||
                             strcasecmp(extension, ".png") == 0 ||
                             strcasecmp(extension, ".gif") == 0)) {
                                // Ensure non-negative file size and prevent
                                // integer overflow
                                if (file_stats.st_size < 0)
                                        continue;

                                // Optional: impose max file size limit, e.g.,
                                // 100 MB
                                const off_t MAX_FILE_SIZE = 100 * 1024 * 1024;
                                if (file_stats.st_size > MAX_FILE_SIZE)
                                        continue;

                                if (file_stats.st_size > *largest_file_size) {
                                        *largest_file_size = file_stats.st_size;

                                        // Free previous allocation if owned
                                        if (largest_image_file != NULL) {
                                                free(largest_image_file);
                                                largest_image_file = NULL;
                                        }

                                        largest_image_file = strdup(resolved_path);
                                        if (largest_image_file == NULL) {
                                                fprintf(stderr,
                                                        "Memory allocation "
                                                        "failure\n");
                                                // Return early or continue
                                                // depending on desired behavior
                                                break;
                                        }
                                }
                        }
                }
        }

        closedir(directory);
        return largest_image_file;
}

gchar *generate_track_id(void)
{
        gchar *track_id =
            g_strdup_printf("/org/kew/tracklist/track%d", track_counter);
        track_counter++;
        return track_id;
}

int load_color(SongData *songdata)
{
        return get_cover_color(songdata->cover, songdata->coverWidth,
                               songdata->coverHeight, &(songdata->red),
                               &(songdata->green), &(songdata->blue));
}

void load_meta_data(SongData *songdata)
{
        AppState *state = get_app_state();
        char path[PATH_MAX];

        songdata->metadata = malloc(sizeof(TagSettings));
        if (songdata->metadata == NULL) {
                songdata->hasErrors = true;
                return;
        }
        songdata->metadata->replaygainTrack = 0.0;
        songdata->metadata->replaygainAlbum = 0.0;

        generate_temp_file_path(songdata->cover_art_path, "cover", ".jpg");

        int res = extractTags(songdata->file_path, songdata->metadata,
                              &(songdata->duration), songdata->cover_art_path, &(songdata->lyrics));

        if (res == -2) {
                songdata->hasErrors = true;
                return;
        } else if (res == -1) {
                get_directory_from_path(songdata->file_path, path);
                char *tmp = NULL;
                off_t size = 0;
                char *file_arr[12] = {
                    "front.png",
                    "front.jpg",
                    "front.jpeg",
                    "folder.png",
                    "folder.jpg",
                    "folder.jpeg",
                    "cover.png",
                    "cover.jpg",
                    "cover.jpeg",
                    "f.png",
                    "f.jpg",
                    "f.jpeg",
                };
                tmp = choose_album_art(path, file_arr, 12, 0);
                if (tmp == NULL) {
                        tmp = find_largest_image_file(path, tmp, &size);
                }

                if (tmp != NULL) {
                        c_strcpy(songdata->cover_art_path, tmp,
                                 sizeof(songdata->cover_art_path));
                        free(tmp);
                        tmp = NULL;
                } else
                        c_strcpy(songdata->cover_art_path, "",
                                 sizeof(songdata->cover_art_path));
        } else {
                add_to_cache(state->tmpCache, songdata->cover_art_path);
        }

        songdata->cover =
            get_bitmap(songdata->cover_art_path, &(songdata->coverWidth),
                       &(songdata->coverHeight));
}

void unload_lyrics(SongData *songdata)
{
        if (songdata->lyrics) {
                freeLyrics(songdata->lyrics);
                songdata->lyrics = NULL;
        }
}

SongData *load_song_data(char *file_path)
{
        AppState *state = get_app_state();
        SongData *songdata = NULL;
        songdata = malloc(sizeof(SongData));
        songdata->magic = SONG_MAGIC;
        songdata->track_id = generate_track_id();
        songdata->hasErrors = false;
        c_strcpy(songdata->file_path, "", sizeof(songdata->file_path));
        c_strcpy(songdata->cover_art_path, "", sizeof(songdata->cover_art_path));
        songdata->red = state->uiSettings.defaultColorRGB.r;
        songdata->green = state->uiSettings.defaultColorRGB.g;
        songdata->blue = state->uiSettings.defaultColorRGB.b;
        songdata->metadata = NULL;
        songdata->cover = NULL;
        songdata->duration = 0.0;
        songdata->avg_bit_rate = 0;
        songdata->lyrics = NULL;
        c_strcpy(songdata->file_path, file_path, sizeof(songdata->file_path));
        songdata->lyrics = loadLyricsFromLRC(songdata->file_path);
        load_meta_data(songdata);
        int res = load_color(songdata);

        if (songdata->cover && res != 0) {
                songdata->red = state->uiSettings.defaultColorRGB.r;
                songdata->green = state->uiSettings.defaultColorRGB.g;
                songdata->blue = state->uiSettings.defaultColorRGB.b;
        }

        return songdata;
}

void unload_song_data(SongData **songdata)
{
        AppState *state = get_app_state();
        if (*songdata == NULL)
                return;

        SongData *data = *songdata;

        if (data->cover != NULL) {
                stbi_image_free(data->cover);
                data->cover = NULL;
        }

        if (exists_in_cache(state->tmpCache, data->cover_art_path) &&
            is_in_temp_dir(data->cover_art_path)) {
                delete_file(data->cover_art_path);
        }

        unload_lyrics(data);

        data->magic = 0;

        free(data->metadata);
        free(data->track_id);

        data->cover = NULL;
        data->metadata = NULL;

        data->track_id = NULL;

        free(*songdata);
        *songdata = NULL;
}
