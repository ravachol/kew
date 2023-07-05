#include "songloader.h"

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
    else {
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
    songdata->duration = (double*)malloc(sizeof(double)); 
    *(songdata->duration) = getDuration(songdata->filePath);
}

void loadPcmAudio(SongData *songdata)
{
    generateTempFilePath(songdata->pcmFilePath, "temp", ".pcm");
    convertToPcmFile(songdata->filePath, songdata->pcmFilePath);
    addToCache(tempCache, songdata->pcmFilePath);
}

SongData* loadSongData(char *filePath)
{
    SongData* songdata = malloc(sizeof(SongData));
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

void unloadSongData(SongData *songdata)
{
    if (songdata == NULL)
        return;
    if (songdata->cover != NULL)
        FreeImage_Unload(songdata->cover); 
    free(songdata->red);
    free(songdata->green);
    free(songdata->blue); 
	free(songdata->metadata);
    free(songdata->duration);
    songdata->cover = NULL;
    songdata->red = NULL;
    songdata->green = NULL;
    songdata->blue = NULL;    
    songdata->metadata = NULL;
    songdata->duration = NULL;
    if (existsFile(songdata->pcmFilePath))
        deleteFile(songdata->pcmFilePath);  
    if (existsFile(songdata->coverArtPath))
        deleteFile(songdata->coverArtPath);
    if (songdata->pcmFile != NULL)
      free(songdata->pcmFile);
    songdata->pcmFile = NULL;        
    
    songdata = NULL;
}