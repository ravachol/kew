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
#define MAX_FFMPEG_PROCESSES 10

pid_t ffmpegPids[MAX_FFMPEG_PROCESSES];
int numRunningProcesses = 0;

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

void *child_cleanup(void *arg)
{
        thread_data *data = (thread_data *)arg;
        int status;
        waitpid(data->pid, &status, 0);
        ffmpegPids[data->index] = -1;
        numRunningProcesses--;
        free(arg);
        return NULL;
}

void stopFFmpeg()
{
        for (int i = 0; i < numRunningProcesses; i++)
        {
                pid_t pid = ffmpegPids[i];
                if (pid != -1)
                {
                        if (kill(pid, SIGINT) == 0)
                        {
                                ffmpegPids[i] = -1;
                                waitpid(pid, NULL, 0);
                        }
                        else
                        {
                                if (kill(ffmpegPid, SIGTERM) == 0)
                                {
                                        ffmpegPids[i] = -1;
                                        waitpid(pid, NULL, 0);
                                }
                        }
                }
        }
}

int convertToPcmFile(SongData* songData, const char *filePath, const char *outputFilePath)
{
        char command[COMMAND_SIZE];

        char *escapedInputFilePath = escapeFilePath(filePath);

        snprintf(command, sizeof(command),
                 "ffmpeg -v fatal -hide_banner -nostdin -y -i \"%s\" -f s24le -acodec pcm_s24le -ac %d -ar %d -threads auto \"%s\"",
                 escapedInputFilePath, CHANNELS, SAMPLE_RATE, outputFilePath);

        free(escapedInputFilePath);

        pid_t currentPid = fork();
        switch (currentPid)
        {
        case -1:
        {
                perror("fork failed");
                return -1;
        }
        case 0:
        {
                // Child process
                if (setsid() == -1)
                {
                        perror("setsid failed");
                        exit(EXIT_FAILURE);
                }

                close(STDIN_FILENO);
                close(STDOUT_FILENO);
                close(STDERR_FILENO);

                char *args[] = {"sh", "-c", command, NULL};
                execvp("sh", args);

                perror("execvp failed");
                exit(EXIT_FAILURE);
        }
        default:
        {
                // Parent process
                pthread_t thread_id;
                thread_data *data = malloc(sizeof(thread_data));
                if (data == NULL)
                {
                        perror("malloc failed");
                        return -1;
                }
                ffmpegPids[numRunningProcesses] = currentPid;
                data->pid = currentPid;
                data->index = numRunningProcesses;
                numRunningProcesses++;

                if (pthread_create(&thread_id, NULL, child_cleanup, data) != 0)
                {
                        perror("pthread_create failed");
                        free(data);
                        return -1;
                }
                if (pthread_detach(thread_id) != 0)
                {
                        perror("pthread_detach failed");
                        free(data);
                        return -1;
                }
        }
        }

        return 0;
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

        if (result == -1)
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

int loadPcmAudio(SongData *songdata)
{
        // Don't go through the trouble if this is already a failed file
        if (songdata->hasErrors)
                return -1;

        generateTempFilePath(songdata->filePath, songdata->pcmFilePath, "temp", ".pcm");
        convertToPcmFile(songdata, songdata->filePath, songdata->pcmFilePath);
        int count = 0;
        int result = -1;
        while (result < 1 && count < maxSleepTimes)
        {
                result = existsFile(songdata->pcmFilePath);
                c_sleep(sleepAmount);
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
        songdata->deleted = false;
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
        if (!hasBuiltinDecoder(songdata->filePath) && 
        !endsWith(filePath, "opus") && 
        !endsWith(filePath, "ogg"))
                loadPcmAudio(songdata);
        return songdata;
}

void unloadSongData(SongData **songdata)
{
        if (*songdata == NULL || (*songdata)->deleted == true)
                return;

        SongData *data = *songdata;

        data->deleted = true;

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

        if (existsFile(data->pcmFilePath) > -1)
        {
                while (numRunningProcesses > 0)
                        c_sleep(100);
                deleteFile(data->pcmFilePath);
        }
        if (data->pcmFile != NULL)
                free(data->pcmFile);

        free(*songdata);
        *songdata = NULL;
}
