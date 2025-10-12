/**
 * @file songloader.c
 * @brief Song loading and preparation routines.
 *
 * Responsible for preparing a song for playback, including reading metadata,
 * initializing decoders, and allocating playback buffers. Acts as the bridge
 * between playlist entries and the playback subsystem.
 */

#include "songloader.h"

#include "imgfunc.h"
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

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

#define MAX_RECURSION_DEPTH 10

static guint track_counter = 0;

void makeFilePath(const char *dirPath, char *filePath, size_t filePathSize,
                  const struct dirent *entry)
{
        if (dirPath == NULL || filePath == NULL || entry == NULL ||
            filePathSize == 0)
                return;

        size_t dirLen = strnlen(dirPath, filePathSize);
        size_t nameLen = strnlen(entry->d_name, filePathSize);

        if (dirLen == filePathSize)
        {
                filePath[0] = '\0';
                return;
        }

        size_t neededSize = dirLen + nameLen + 1; // +1 for '\0'
        if (dirPath[dirLen - 1] != '/')
        {
                neededSize += 1; // for the added '/'
        }

        if (neededSize > filePathSize)
        {
                filePath[0] = '\0';
                return;
        }

        // Compose the path safely
        if (dirPath[dirLen - 1] == '/')
        {
                snprintf(filePath, filePathSize, "%s%s", dirPath,
                         entry->d_name);
        }
        else
        {
                snprintf(filePath, filePathSize, "%s/%s", dirPath,
                         entry->d_name);
        }

        // snprintf guarantees null termination if filePathSize > 0
}

char *chooseAlbumArt(const char *dirPath, char **customFileNameArr, int size,
                     int depth)
{
        if (!dirPath || !customFileNameArr || size <= 0 ||
            depth > MAX_RECURSION_DEPTH)
        {
                return NULL;
        }

        DIR *directory = opendir(dirPath);
        if (!directory)
        {
                return NULL;
        }

        struct dirent *entry;
        struct stat fileStat;
        char filePath[MAXPATHLEN];
        char resolvedPath[MAXPATHLEN];
        char *result = NULL;

        for (int i = 0; i < size && !result; i++)
        {
                rewinddir(directory);
                while ((entry = readdir(directory)) != NULL)
                {
                        if (strcmp(entry->d_name, ".") == 0 ||
                            strcmp(entry->d_name, "..") == 0)
                                continue;

                        int written = snprintf(filePath, sizeof(filePath),
                                               "%s/%s", dirPath, entry->d_name);
                        if (written < 0 || written >= (int)sizeof(filePath))
                        {
                                continue; // path too long
                        }

                        if (realpath(filePath, resolvedPath) == NULL)
                        {
                                continue;
                        }

                        if (strncmp(resolvedPath, dirPath, strlen(dirPath)) !=
                            0)
                        {
                                continue; // outside allowed directory
                        }

                        if (stat(resolvedPath, &fileStat) == 0 &&
                            S_ISREG(fileStat.st_mode))
                        {
                                if (strcmp(entry->d_name,
                                           customFileNameArr[i]) == 0)
                                {
                                        result = strdup(resolvedPath);
                                        break;
                                }
                        }
                }
        }

        // Recursive search for directories
        if (!result)
        {
                rewinddir(directory);
                while ((entry = readdir(directory)) != NULL && !result)
                {
                        if (strcmp(entry->d_name, ".") == 0 ||
                            strcmp(entry->d_name, "..") == 0)
                                continue;

                        int written = snprintf(filePath, sizeof(filePath),
                                               "%s/%s", dirPath, entry->d_name);
                        if (written < 0 || written >= (int)sizeof(filePath))
                        {
                                continue;
                        }

                        if (realpath(filePath, resolvedPath) == NULL)
                        {
                                continue;
                        }

                        if (strncmp(resolvedPath, dirPath, strlen(dirPath)) !=
                            0)
                        {
                                continue;
                        }

                        struct stat linkStat;
                        if (lstat(resolvedPath, &linkStat) == 0)
                        {
                                if (S_ISLNK(linkStat.st_mode))
                                {
                                        continue; // skip symlink
                                }
                                if (S_ISDIR(linkStat.st_mode))
                                {
                                        result = chooseAlbumArt(
                                            resolvedPath, customFileNameArr,
                                            size, depth + 1);
                                }
                        }
                }
        }

        closedir(directory);
        return result;
}

char *findLargestImageFile(const char *directoryPath, char *largestImageFile,
                           off_t *largestFileSize)
{
        DIR *directory = opendir(directoryPath);
        if (directory == NULL)
        {
                fprintf(stderr, "Failed to open directory: %s\n",
                        directoryPath);
                return largestImageFile;
        }

        struct dirent *entry;
        struct stat fileStats;
        char filePath[MAXPATHLEN];
        char resolvedPath[MAXPATHLEN];

        while ((entry = readdir(directory)) != NULL)
        {
                // Skip "." and ".."
                if (strcmp(entry->d_name, ".") == 0 ||
                    strcmp(entry->d_name, "..") == 0)
                        continue;

                // Construct file path safely
                int len = snprintf(filePath, sizeof(filePath), "%s/%s",
                                   directoryPath, entry->d_name);
                if (len < 0 || len >= (int)sizeof(filePath))
                {
                        // Path too long, skip
                        continue;
                }

                // Resolve the real path
                if (realpath(filePath, resolvedPath) == NULL)
                {
                        // Could not resolve, skip
                        continue;
                }

                // Use lstat to avoid following symlinks
                if (lstat(resolvedPath, &fileStats) == -1)
                {
                        continue;
                }

                if (S_ISLNK(fileStats.st_mode))
                {
                        // Ignore symlinks
                        continue;
                }

                if (S_ISREG(fileStats.st_mode))
                {
                        // Validate extension
                        char *extension = strrchr(entry->d_name, '.');
                        if (extension != NULL &&
                            (strcasecmp(extension, ".jpg") == 0 ||
                             strcasecmp(extension, ".jpeg") == 0 ||
                             strcasecmp(extension, ".png") == 0 ||
                             strcasecmp(extension, ".gif") == 0))
                        {
                                // Ensure non-negative file size and prevent
                                // integer overflow
                                if (fileStats.st_size < 0)
                                        continue;

                                // Optional: impose max file size limit, e.g.,
                                // 100 MB
                                const off_t MAX_FILE_SIZE = 100 * 1024 * 1024;
                                if (fileStats.st_size > MAX_FILE_SIZE)
                                        continue;

                                if (fileStats.st_size > *largestFileSize)
                                {
                                        *largestFileSize = fileStats.st_size;

                                        // Free previous allocation if owned
                                        if (largestImageFile != NULL)
                                        {
                                                free(largestImageFile);
                                                largestImageFile = NULL;
                                        }

                                        largestImageFile = strdup(resolvedPath);
                                        if (largestImageFile == NULL)
                                        {
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
        return largestImageFile;
}

gchar *generateTrackId(void)
{
        gchar *trackId =
            g_strdup_printf("/org/kew/tracklist/track%d", track_counter);
        track_counter++;
        return trackId;
}

void loadColor(SongData *songdata)
{
        getCoverColor(songdata->cover, songdata->coverWidth,
                      songdata->coverHeight, &(songdata->red),
                      &(songdata->green), &(songdata->blue));
}

void loadMetaData(SongData *songdata)
{
        AppState *state = getAppState();
        char path[MAXPATHLEN];

        songdata->metadata = malloc(sizeof(TagSettings));
        if (songdata->metadata == NULL)
        {
                songdata->hasErrors = true;
                return;
        }
        songdata->metadata->replaygainTrack = 0.0;
        songdata->metadata->replaygainAlbum = 0.0;

        generateTempFilePath(songdata->coverArtPath, "cover", ".jpg");

        int res = extractTags(songdata->filePath, songdata->metadata,
                              &(songdata->duration), songdata->coverArtPath);

        if (res == -2)
        {
                songdata->hasErrors = true;
                return;
        }
        else if (res == -1)
        {
                getDirectoryFromPath(songdata->filePath, path);
                char *tmp = NULL;
                off_t size = 0;
                char *fileArr[12] = {
                    "front.png",  "front.jpg",   "front.jpeg", "folder.png",
                    "folder.jpg", "folder.jpeg", "cover.png",  "cover.jpg",
                    "cover.jpeg", "f.png",       "f.jpg",      "f.jpeg",
                };
                tmp = chooseAlbumArt(path, fileArr, 12, 0);
                if (tmp == NULL)
                {
                        tmp = findLargestImageFile(path, tmp, &size);
                }

                if (tmp != NULL)
                {
                        c_strcpy(songdata->coverArtPath, tmp,
                                 sizeof(songdata->coverArtPath));
                        free(tmp);
                        tmp = NULL;
                }
                else
                        c_strcpy(songdata->coverArtPath, "",
                                 sizeof(songdata->coverArtPath));
        }
        else
        {
                addToCache(state->tmpCache, songdata->coverArtPath);
        }

        songdata->cover =
            getBitmap(songdata->coverArtPath, &(songdata->coverWidth),
                      &(songdata->coverHeight));
}

void unloadLyrics(SongData *songdata)
{
        if (songdata->lyrics)
        {
                freeLyrics(songdata->lyrics);
                songdata->lyrics = NULL;
        }
}

SongData *loadSongData(char *filePath)
{
        AppState *state = getAppState();
        SongData *songdata = NULL;
        songdata = malloc(sizeof(SongData));
        songdata->trackId = generateTrackId();
        songdata->hasErrors = false;
        c_strcpy(songdata->filePath, "", sizeof(songdata->filePath));
        c_strcpy(songdata->coverArtPath, "", sizeof(songdata->coverArtPath));
        songdata->red = state->uiSettings.kewColorRGB.r;
        songdata->green = state->uiSettings.kewColorRGB.g;
        songdata->blue = state->uiSettings.kewColorRGB.b;
        songdata->metadata = NULL;
        songdata->cover = NULL;
        songdata->duration = 0.0;
        songdata->avgBitRate = 0;
        c_strcpy(songdata->filePath, filePath, sizeof(songdata->filePath));
        loadMetaData(songdata);
        songdata->lyrics = loadLyrics(songdata->filePath);
        loadColor(songdata);

        return songdata;
}

void unloadSongData(SongData **songdata)
{
        AppState *state = getAppState();
        if (*songdata == NULL)
                return;

        SongData *data = *songdata;

        if (data->cover != NULL)
        {
                stbi_image_free(data->cover);
                data->cover = NULL;
        }

        if (existsInCache(state->tmpCache, data->coverArtPath) &&
            isInTempDir(data->coverArtPath))
        {
                deleteFile(data->coverArtPath);
        }

        unloadLyrics(data);

        free(data->metadata);
        free(data->trackId);

        data->cover = NULL;
        data->metadata = NULL;

        data->trackId = NULL;

        free(*songdata);
        *songdata = NULL;
}
