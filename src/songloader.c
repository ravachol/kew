#include "songloader.h"

static guint track_counter = 0;

// Generate a new track ID
gchar* generateTrackId() {
    gchar* trackId = g_strdup_printf("/org/cueMusic/tracklist/track%d", track_counter);
    track_counter++;
    return trackId;
}    

void loadCover(SongData *songdata)
{
    char path[MAXPATHLEN];

    if (strlen(songdata->filePath) == 0)
        return;

    generateTempFilePath(songdata->coverArtPath, "cover", ".jpg");
    int res = extractCoverCommand(songdata->filePath, songdata->coverArtPath);
    if (res < 0)
    {
        getDirectoryFromPath(songdata->filePath, path);
        char *tmp = NULL;
        off_t size = 0;
        tmp = findLargestImageFileRecursive(path, tmp, &size);

        if (tmp != NULL)
            strcpy(songdata->coverArtPath, tmp);
        else
            return;
    }
    else
    {
        addToCache(tempCache, songdata->coverArtPath);
    }
    songdata->cover = getBitmap(songdata->coverArtPath);
}

void loadColor(SongData *songdata)
{
    getCoverColor(songdata->cover, &(songdata->red), &(songdata->green), &(songdata->blue));
}

void loadMetaData(SongData *songdata)
{
    songdata->metadata = malloc(sizeof(TagSettings));
    extractTags(songdata->filePath, songdata->metadata);
}

void loadDuration(SongData *songdata)
{
    songdata->duration = (double *)malloc(sizeof(double));
    *(songdata->duration) = getDuration(songdata->filePath);
}

void loadPcmAudio(SongData *songdata)
{
    generateTempFilePath(songdata->pcmFilePath, "temp", ".pcm");
    convertToPcmFile(songdata->filePath, songdata->pcmFilePath);
    while (!existsFile(songdata->pcmFilePath))
    {
        usleep(500000);
    }
    addToCache(tempCache, songdata->pcmFilePath);
}

SongData *loadSongData(char *filePath)
{
    SongData *songdata = malloc(sizeof(SongData));
    songdata->trackId = generateTrackId();    
    strcpy(songdata->filePath, "");
    strcpy(songdata->coverArtPath, "");
    strcpy(songdata->pcmFilePath, "");
    songdata->red = NULL;
    songdata->green = NULL;
    songdata->blue = NULL;
    songdata->metadata = NULL;
    songdata->cover = NULL;
    songdata->duration = NULL;
    songdata->pcmFile = NULL;
    songdata->pcmFileSize = 0;
    strcpy(songdata->filePath, filePath);
    loadCover(songdata);
    usleep(10000);
    loadColor(songdata);
    usleep(10000);
    loadMetaData(songdata);
    usleep(10000);
    loadDuration(songdata);
    usleep(10000);
    loadPcmAudio(songdata);
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

    free(data->red);
    free(data->green);
    free(data->blue);
    free(data->metadata);
    free(data->duration);

    data->cover = NULL;
    data->red = NULL;
    data->green = NULL;
    data->blue = NULL;
    data->metadata = NULL;
    data->duration = NULL;

    if (existsFile(data->pcmFilePath))
        deleteFile(data->pcmFilePath);

    if (data->pcmFile != NULL)
        free(data->pcmFile);

    free(*songdata);
    *songdata = NULL;
}