#include "songloader.h"
/*

songloader.c

 This file should contain only functions related to loading song data.

*/

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

static guint track_counter = 0;

// Generate a new track ID
gchar *generateTrackId()
{
        gchar *trackId = g_strdup_printf("/org/kew/tracklist/track%d", track_counter);
        track_counter++;
        return trackId;
}

void loadColor(SongData *songdata)
{
        getCoverColor(songdata->cover, &(songdata->red), &(songdata->green), &(songdata->blue));
}

void loadMetaData(SongData *songdata)
{
        char path[MAXPATHLEN];

        songdata->metadata = malloc(sizeof(TagSettings));
        generateTempFilePath(songdata->filePath, songdata->coverArtPath, "cover", ".jpg");
        int res = extractTags(songdata->filePath, songdata->metadata, &songdata->duration, songdata->coverArtPath);

        if (res < 0)
        {
                getDirectoryFromPath(songdata->filePath, path);
                char *tmp = NULL;
                off_t size = 0;
                tmp = findLargestImageFile(path, tmp, &size);

                if (tmp != NULL)
                        c_strcpy(songdata->coverArtPath, sizeof(songdata->coverArtPath), tmp);
                else
                        return;
        }
        else
        {
                addToCache(tempCache, songdata->coverArtPath);
        }

        songdata->cover = getBitmap(songdata->coverArtPath);
}

SongData *loadSongData(char *filePath)
{
        SongData *songdata = NULL;
        songdata = malloc(sizeof(SongData));
        songdata->trackId = generateTrackId();
        songdata->hasErrors = false;
        c_strcpy(songdata->filePath, sizeof(songdata->filePath), "");
        c_strcpy(songdata->coverArtPath, sizeof(songdata->coverArtPath), "");
        songdata->red = NULL;
        songdata->green = NULL;
        songdata->blue = NULL;
        songdata->metadata = NULL;
        songdata->cover = NULL;
        songdata->duration = 0.0;
        c_strcpy(songdata->filePath, sizeof(songdata->filePath), filePath);       
        loadMetaData(songdata);
        loadColor(songdata);
        return songdata;
}

void unloadSongData(SongData **songdata)
{
        if (*songdata == NULL)
                return;

        SongData *data = *songdata;

        if (data->cover != NULL)
        {
                FreeImage_Unload(data->cover);
                data->cover = NULL;
        }

        if (existsInCache(tempCache, data->coverArtPath))
        {
                deleteFile(data->coverArtPath);
        }

        free(data->red);
        free(data->green);
        free(data->blue);
        free(data->metadata);
        free(data->trackId);

        data->cover = NULL;
        data->red = NULL;
        data->green = NULL;
        data->blue = NULL;
        data->metadata = NULL;

        data->trackId = NULL;

        free(*songdata);
        *songdata = NULL;
}
