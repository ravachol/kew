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
        // Initialisatin of the used variables/objects/structs
        DIR *directory = opendir(directoryPath);
        struct dirent *entry;
        struct stat fileStats;

        // Check if the directory exists
        if (directory == NULL)
        {
                fprintf(stderr, "Failed to open directory: %s\n", directoryPath);
                return largestImageFile;
        }

        // Loop over the files in the directory
        while ((entry = readdir(directory)) != NULL)
        {
                char filePath[MAXPATHLEN];

                if (directoryPath[strnlen(directoryPath, MAXPATHLEN) - 1] == '/')
                {
                        snprintf(filePath, sizeof(filePath), "%s%s", directoryPath, entry->d_name);
                }
                else
                {
                        snprintf(filePath, sizeof(filePath), "%s/%s", directoryPath, entry->d_name);
                }

                // Load file attributes into stat object and abrot if it can't be done
                if (stat(filePath, &fileStats) == -1)
                {
                        continue;
                }

                // Check if file is not a directory
                if (S_ISREG(fileStats.st_mode))
                {
                        // Get the file extension
                        char *extension = strrchr(entry->d_name, '.');

                        // Check if file is a png
                        if (extension != NULL && (strcasecmp(extension, ".png")) == 0)
                        {
                                *largestImageFile = fileStats.st_size;
                        }

                        // Check if file is a jpg
                        else if (extension != NULL && (strcasecmp(extension, ".jpg")) == 0)
                        {
                                *largestImageFile = fileStats.st_size;
                        }

                        // Check if file is jpeg
                        else if (extension != NULL && (strcasecmp(extension, ".jepg")) == 0)
                        {
                                *largestImageFile = fileStats.st_size;
                        }

                        // Check if file is gif
                        else if (extension != NULL && (strcasecmp(extension, ".gif")) == 0)
                        {
                                *largestImageFile = fileStats.st_size;
                        }


                        // If file doesn't exist, free up the memory
                        if (largestImageFile != NULL)
                        {
                                free(largestImageFile);
                        }

                }
        }

        closedir(directory);
        return largestImageFile;
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
