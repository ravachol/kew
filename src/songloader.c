#include "songloader.h"

static guint track_counter = 0;

// Generate a new track ID
gchar *generateTrackId()
{
    gchar *trackId = g_strdup_printf("/org/cueMusic/tracklist/track%d", track_counter);
    track_counter++;
    return trackId;
}

void loadCover(SongData *songdata)
{
    char path[MAXPATHLEN];

    if (strlen(songdata->filePath) == 0)
        return;

    generateTempFilePath(songdata->filePath, songdata->coverArtPath, "cover", ".jpg");
    int res = extractCoverCommand(songdata->filePath, songdata->coverArtPath);
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

    // if (existsFile(songdata->coverArtPath))
    // {
    //     // Limit the permissions on this file to current user
    //     mode_t mode = S_IRUSR | S_IWUSR;
    //     chmod(songdata->coverArtPath, mode);
    // }    
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
    int result = getDuration(songdata->filePath);
    *(songdata->duration) = result;

    if (result == -1)
        songdata->hasErrors = true;
}

int loadPcmAudio(SongData *songdata)
{
    // Don't go through the trouble if this is already a failed file
    if (songdata->hasErrors)
        return -1;  

    generateTempFilePath(songdata->filePath, songdata->pcmFilePath, "temp", ".pcm");    
    convertToPcmFile(songdata->filePath, songdata->pcmFilePath);
    int count = 0;
    int result = -1;
    while (result < 1 && count < 10)
    {
        result = existsFile(songdata->pcmFilePath);
        c_sleep(300);
        count++;
    }

    // File doesn't exist
    if (result < 1)
    {
        songdata->hasErrors = true;
        return -1;
    }
    else
    {
        // Limit the permissions on this file to current user
        mode_t mode = S_IRUSR | S_IWUSR;
        chmod(songdata->pcmFilePath, mode);
    }    

    addToCache(tempCache, songdata->pcmFilePath);

    return 0;
}

SongData *loadSongData(char *filePath)
{
    SongData *songdata = malloc(sizeof(SongData));
    songdata->trackId = generateTrackId();
    songdata->hasErrors = false;
    c_strcpy(songdata->filePath, sizeof(songdata->filePath), "");
    c_strcpy(songdata->coverArtPath, sizeof(songdata->coverArtPath), "");
    c_strcpy(songdata->pcmFilePath, sizeof(songdata->pcmFilePath), "");
    songdata->red = NULL;
    songdata->green = NULL;
    songdata->blue = NULL;
    songdata->metadata = NULL;
    songdata->cover = NULL;
    songdata->duration = NULL;
    songdata->pcmFile = NULL;
    songdata->pcmFileSize = 0;
    c_strcpy(songdata->filePath, sizeof(songdata->filePath), filePath);
    loadCover(songdata);
    c_sleep(10);
    loadColor(songdata);
    c_sleep(10);
    loadMetaData(songdata);
    c_sleep(10);
    loadDuration(songdata);
    c_sleep(10);
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