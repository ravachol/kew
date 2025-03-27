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

char *findLargestImageFile(const char *directoryPath, char *largestImageFile, off_t *largestFileSize)
{
        DIR *directory = opendir(directoryPath);
        struct dirent *entry;
        struct stat fileStats;

        if (directory == NULL)
        {
                fprintf(stderr, "Failed to open directory: %s\n", directoryPath);
                return largestImageFile;
        }

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

                if (stat(filePath, &fileStats) == -1)
                {
                        continue;
                }

                if (S_ISREG(fileStats.st_mode))
                {
                        // Check if the entry is an image file and has a larger size than the current largest image file
                        char *extension = strrchr(entry->d_name, '.');
                        if (extension != NULL && (strcasecmp(extension, ".jpg") == 0 || strcasecmp(extension, ".jpeg") == 0 ||
                                                  strcasecmp(extension, ".png") == 0 || strcasecmp(extension, ".gif") == 0))
                        {
                                if (fileStats.st_size > *largestFileSize)
                                {
                                        *largestFileSize = fileStats.st_size;
                                        if (largestImageFile != NULL)
                                        {
                                                free(largestImageFile);
                                        }
                                        largestImageFile = strdup(filePath);
                                }
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

        int res = extractTags(songdata->filePath, songdata->metadata, &songdata->duration, songdata->coverArtPath);

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
                tmp = findLargestImageFile(path, tmp, &size);

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
                addToCache(state->tempCache, songdata->coverArtPath);
        }

        songdata->cover = getBitmap(songdata->coverArtPath, &songdata->coverWidth, &songdata->coverHeight);
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

        if (existsInCache(state->tempCache, data->coverArtPath) && isInTempDir(data->coverArtPath))
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
