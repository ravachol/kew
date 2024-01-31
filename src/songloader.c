#include "songloader.h"
/*

songloader.c

 This file should contain only functions related to loading song data.

*/

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

static guint track_counter = 0;
int ffmpegPid = -1;

#define COMMAND_SIZE 1000

int maxSleepTimes = 10;
int sleepAmount = 300;

typedef struct thread_data
{
        pid_t pid;
        int index;
} thread_data;

// Generate a new track ID
gchar *generateTrackId()
{
        gchar *trackId = g_strdup_printf("/org/kew/tracklist/track%d", track_counter);
        track_counter++;
        return trackId;
}

int getSongDuration(const char *filePath, double *duration)
{
        char command[1024];
        FILE *pipe;
        char output[1024];
        char *durationStr;
        double durationValue;

        char *escapedInputFilePath = escapeFilePath(filePath);

        snprintf(command, sizeof(command),
                 "ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 \"%s\"",
                 escapedInputFilePath);

        free(escapedInputFilePath);

        pipe = popen(command, "r");
        if (pipe == NULL)
        {
                fprintf(stderr, "Failed to run FFprobe command.\n");
                return -1;
        }
        if (fgets(output, sizeof(output), pipe) == NULL)
        {
                fprintf(stderr, "Failed to read FFprobe output.\n");
                pclose(pipe);
                return -1;
        }
        pclose(pipe);

        durationStr = strtok(output, "\n");
        if (durationStr == NULL)
        {
                fprintf(stderr, "Failed to parse FFprobe output.\n");
                return -1;
        }
        durationValue = atof(durationStr);
        *duration = durationValue;
        return 0;
}

double getDuration(const char *filepath)
{
        double duration = 0.0;
        int result = getSongDuration(filepath, &duration);

        if (result == -1 || duration == 0)
                return -1;
        else
                return duration;
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
        songdata->duration = NULL;
        c_strcpy(songdata->filePath, sizeof(songdata->filePath), filePath);
        loadCover(songdata);
        c_sleep(10);
        loadColor(songdata);
        c_sleep(10);
        loadMetaData(songdata);
        c_sleep(10);
        loadDuration(songdata);
        c_sleep(10);

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
        free(data->duration);
        free(data->trackId);

        data->cover = NULL;
        data->red = NULL;
        data->green = NULL;
        data->blue = NULL;
        data->metadata = NULL;
        data->duration = NULL;

        data->trackId = NULL;

        free(*songdata);
        *songdata = NULL;
}
