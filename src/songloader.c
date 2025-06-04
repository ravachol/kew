#include <glib.h>
#include <gio/gio.h>
#include <sys/wait.h>
#include <unistd.h>
#include "tagLibWrapper.h"
#include "cache.h"
#include "imgfunc.h"
#include "file.h"
#include "sound.h"
#include "soundcommon.h"
#include "utils.h"
#include "songloader.h"
#include "stb_image.h"
/*

songloader.c

 This file should contain only functions related to loading song data.

*/

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

static guint track_counter = 0;

char *findImageFile(const char *directoryPath, char *largestImageFile, off_t *largestFileSize)
{
        DIR *directory = opendir(directoryPath);
        struct dirent *entry;
        char filePath[PATH_MAX];
        char *preferredImage = NULL;

        if (directory == NULL)
        {
                fprintf(stderr, "Failed to open directory: %s\n", directoryPath);
                return NULL;
        }

        // Arrays of preference order
        const char *priorityList[] = {
                "front.png",
                "front.jpg",
                "front.jpeg",
                "folder.png",
                "folder.jpg",
                "folder.jpeg"
                "cover.png",
                "cover.jpg",
                "cover.jpeg",
                NULL // Placeholder for wildcard matches
        };

        // Calculate and store the length of the array `priorityList` into `priorityCount`
        size_t priorityCount = sizeof(priorityList) / sizeof(priorityList[0]);

        // Initialize arrays to store wildcard matches by type
        char *pngMatch = NULL;
        char *jpgMatch = NULL;
        char *jpegMatch = NULL;
        char *gifMatch = NULL;

        // Loop over the files
        while ((entry = readdir(directory)) != NULL)
        {
                // Skip non-regular files
                if (entry->d_type != DT_REG)
                {
                        continue;
                }

                // Combine directory path and file name into full file path, storing it in `filePath`
                snprintf(filePath, sizeof(filePath), "%s/%s", directoryPath, entry->d_name);

                // Loop over the files and check if they match an entry in the `priorityList`
                for (size_t i = 0; i < priorityCount - 1; ++i)
                {
                        // Check if filename matches a entry in the `priorityList`
                        if (strcasecmp(entry->d_name, priorityList[i]) == 0)
                        {
                                preferredImage = strdup(filePath);
                                closedir(directory);
                                return preferredImage;
                        }
                }

                // Store the last dot and everything after in `*extension`
                char *extension = strrchr(entry->d_name, '.');

                // Loop over the files and check if they match a given file extension
                if (extension != NULL)
                {
                        if (strcasecmp(extension, ".png") == 0 && pngMatch == NULL)
                        {
                                pngMatch = strdup(filePath);
                        }
                        else if (strcasecmp(extension, ".jpg") == 0 && jpgMatch == NULL)
                        {
                                jpgMatch = strdup(filePath);
                        }
                        else if (strcasecmp(extension, ".jpeg") == 0 && jpegMatch == NULL)
                        {
                                jpegMatch = strdup(filePath);
                        }
                        else if (strcasecmp(extension, ".gif") == 0 && gifMatch == NULL)
                        {
                                gifMatch = strdup(filePath);
                        }
                }
        }

        closedir(directory);

        // Search for the image in the order png>jpg>jpeg>gif
        if (pngMatch)
        {
                preferredImage = pngMatch;
        }
        else if (jpgMatch)
        {
                preferredImage = jpgMatch;
        }
        else if (jpegMatch)
        {
                preferredImage = jpegMatch;
        }
        else if (gifMatch)
        {
                preferredImage = gifMatch;
        }

        // If not found, try specific subdirectories: art, scans, covers, artwork, artworks
        if (preferredImage == NULL)
        {
                const char *subdirs[] = { "art", "scans", "covers", "artwork", "artworks" };
                size_t subdirCount = sizeof(subdirs) / sizeof(subdirs[0]);

                for (size_t i = 0; i < subdirCount; ++i)
                {
                        char subdirPath[PATH_MAX];
                        snprintf(subdirPath, sizeof(subdirPath), "%s/%s", directoryPath, subdirs[i]);

                        // Recursively call this function for subdirectory
                        preferredImage = findImageFile(subdirPath, largestImageFile, largestFileSize);
                        if (preferredImage != NULL)
                        {
                                return preferredImage;
                        }
                }
        }

    return preferredImage;
}

// Generate a new track ID
gchar *generateTrackId(void)
{
        gchar *trackId = g_strdup_printf("/org/kew/tracklist/track%d", track_counter);
        track_counter++;
        return trackId;
}

void loadColor(SongData *songdata)
{
        getCoverColor(songdata->cover, songdata->coverWidth, songdata->coverHeight, &(songdata->red), &(songdata->green), &(songdata->blue));
}

void loadMetaData(SongData *songdata, AppState *state)
{
        char path[MAXPATHLEN];

        songdata->metadata = malloc(sizeof(TagSettings));
        songdata->metadata->replaygainTrack = 0.0;
        songdata->metadata->replaygainAlbum = 0.0;

        generateTempFilePath(songdata->coverArtPath, "cover", ".jpg");

        int res = extractTags(songdata->filePath, songdata->metadata, &(songdata->duration), songdata->coverArtPath);

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
                tmp = findImageFile(path, tmp, &size);

                if (tmp != NULL)
                {
                        c_strcpy(songdata->coverArtPath, tmp, sizeof(songdata->coverArtPath));
                        free(tmp);
                        tmp = NULL;
                }
                else
                        c_strcpy(songdata->coverArtPath, "", sizeof(songdata->coverArtPath));
        }
        else
        {
                addToCache(state->tmpCache, songdata->coverArtPath);
        }

        songdata->cover = getBitmap(songdata->coverArtPath, &(songdata->coverWidth), &(songdata->coverHeight));
}

SongData *loadSongData(char *filePath, AppState *state)
{
        SongData *songdata = NULL;
        songdata = malloc(sizeof(SongData));
        songdata->trackId = generateTrackId();
        songdata->hasErrors = false;
        c_strcpy(songdata->filePath, "", sizeof(songdata->filePath));
        c_strcpy(songdata->coverArtPath, "", sizeof(songdata->coverArtPath));
        songdata->red = defaultColor;
        songdata->green = defaultColor;
        songdata->blue = defaultColor;
        songdata->metadata = NULL;
        songdata->cover = NULL;
        songdata->duration = 0.0;
        songdata->avgBitRate = 0;
        c_strcpy(songdata->filePath, filePath, sizeof(songdata->filePath));
        loadMetaData(songdata, state);
        loadColor(songdata);
        return songdata;
}

void unloadSongData(SongData **songdata, AppState *state)
{
        if (*songdata == NULL)
                return;

        SongData *data = *songdata;

        if (data->cover != NULL)
        {
                stbi_image_free(data->cover);
                data->cover = NULL;
        }

        if (existsInCache(state->tmpCache, data->coverArtPath) && isInTempDir(data->coverArtPath))
        {
                deleteFile(data->coverArtPath);
        }

        free(data->metadata);
        free(data->trackId);

        data->cover = NULL;
        data->metadata = NULL;

        data->trackId = NULL;

        free(*songdata);
        *songdata = NULL;
}
