#define MA_EXPERIMENTAL__DATA_LOOPING_AND_CHAINING
#define MA_NO_ENGINE
#define MINIAUDIO_IMPLEMENTATION
#include "../include/miniaudio/miniaudio.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "file.h"
#include "soundgapless.h"

#define CHANNELS 2
#define SAMPLE_RATE 192000
#define SAMPLE_WIDTH 3
#define SAMPLE_FORMAT ma_format_s24
#define FRAMES_PER_BUFFER 1024
ma_int32 *g_audioBuffer = NULL;
ma_device device = {0};
ma_context context;
ma_device_config deviceConfig;
PCMFileDataSource pcmDataSource;
PCMFile *pFirstFile = NULL;
bool paused = false;
bool skipToNext = false;
bool repeatEnabled = false;

pid_t pid = -1;
pid_t pid2 = -1;
bool usepid2 = false;

static bool eofReached = false;

static ma_result pcm_file_data_source_read(ma_data_source *pDataSource, void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead)
{
    // Dummy implementation
    (void)pDataSource;
    (void)pFramesOut;
    (void)frameCount;
    (void)pFramesRead;
    return MA_SUCCESS;
}
static ma_result pcm_file_data_source_seek(ma_data_source *pDataSource, ma_uint64 frameIndex)
{
    PCMFileDataSource *pPCMDataSource = (PCMFileDataSource *)pDataSource;
    pPCMDataSource->currentPCMFrame = (ma_uint32)frameIndex;
    return MA_SUCCESS;
}

static ma_result pcm_file_data_source_get_data_format(ma_data_source *pDataSource, ma_format *pFormat, ma_uint32 *pChannels, ma_uint32 *pSampleRate, ma_channel *pChannelMap, size_t channelMapCap)
{
    (void)pChannelMap;
    (void)channelMapCap;
    PCMFileDataSource *pPCMDataSource = (PCMFileDataSource *)pDataSource;
    *pFormat = pPCMDataSource->format;
    *pChannels = pPCMDataSource->channels;
    *pSampleRate = pPCMDataSource->sampleRate;
    return MA_SUCCESS;
}

static ma_result pcm_file_data_source_get_cursor(ma_data_source *pDataSource, ma_uint64 *pCursor)
{
    PCMFileDataSource *pPCMDataSource = (PCMFileDataSource *)pDataSource;
    *pCursor = pPCMDataSource->currentPCMFrame;

    return MA_SUCCESS;
}

static ma_result pcm_file_data_source_get_length(ma_data_source *pDataSource, ma_uint64 *pLength)
{
    PCMFileDataSource *pPCMDataSource = (PCMFileDataSource *)pDataSource;

    // Get the current file based on the current file index
    FILE *currentFile;
    if (pPCMDataSource->pUserData->currentFileIndex == 0)
    {
        currentFile = pPCMDataSource->fileA;
    }
    else
    {
        currentFile = pPCMDataSource->fileB;
    }

    ma_uint64 fileSize;

    // Seek to the end of the current file to get its size
    fseek(currentFile, 0, SEEK_END);
    fileSize = ftell(currentFile);
    fseek(currentFile, 0, SEEK_SET);

    // Calculate the total number of frames in the current file
    ma_uint64 frameCount = fileSize / ma_get_bytes_per_frame(pPCMDataSource->format, pPCMDataSource->channels);
    *pLength = frameCount;

    return MA_SUCCESS;
}

static ma_result pcm_file_data_source_set_looping(ma_data_source *pDataSource, ma_bool32 isLooping)
{
    // Dummy implementation
    (void)pDataSource;
    (void)isLooping;

    return MA_SUCCESS;
}

static ma_data_source_vtable pcm_file_data_source_vtable = {
    pcm_file_data_source_read,
    pcm_file_data_source_seek,
    pcm_file_data_source_get_data_format,
    pcm_file_data_source_get_cursor,
    pcm_file_data_source_get_length,
    pcm_file_data_source_set_looping,
    0 // flags
};

ma_result pcm_file_data_source_init(PCMFileDataSource *pPCMDataSource, const char *filenameA, UserData *pUserData)
{
    pPCMDataSource->pUserData = pUserData;
    pPCMDataSource->filenameA = filenameA;
    pPCMDataSource->format = SAMPLE_FORMAT;
    pPCMDataSource->channels = CHANNELS;
    pPCMDataSource->sampleRate = SAMPLE_RATE;
    pPCMDataSource->currentPCMFrame = 0;
    pPCMDataSource->currentFileIndex = 0;
    pPCMDataSource->fileA = fopen(filenameA, "rb");

    pUserData->pcmFileA.file = pPCMDataSource->fileA;

    return MA_SUCCESS;
}

void activateSwitch(PCMFileDataSource *pPCMDataSource)
{
    skipToNext = false;    
    if (!repeatEnabled)
        pPCMDataSource->currentFileIndex = 1 - pPCMDataSource->currentFileIndex; // Toggle between 0 and 1
    pPCMDataSource->switchFiles = true;
}

void pcm_file_data_source_read_pcm_frames(ma_data_source *pDataSource, void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead)
{
    PCMFileDataSource *pPCMDataSource = (PCMFileDataSource *)pDataSource;
    ma_uint32 framesToRead = (ma_uint32)frameCount;

    ma_uint32 bytesPerFrame = ma_get_bytes_per_frame(pPCMDataSource->format, pPCMDataSource->channels);
    ma_uint32 bytesToRead = framesToRead * bytesPerFrame;
    ma_uint32 framesRead = 0;

    while (framesToRead > 0)
    {
        // Check if a file switch is required
        if (pPCMDataSource->switchFiles)
        {
            pPCMDataSource->switchFiles = false;

            // Close the current file
            FILE *currentFile;
            if (pPCMDataSource->currentFileIndex == 0)
            {
                currentFile = pPCMDataSource->fileB;
                if (currentFile != NULL)
                    fclose(currentFile);
            }
            else
            {
                currentFile = pPCMDataSource->fileA;
                if (currentFile != NULL)                
                    fclose(currentFile);
            }

            // Open the new file
            if (pPCMDataSource->currentFileIndex == 0)
            {
                pPCMDataSource->fileA = (pPCMDataSource->pUserData->pcmFileA.filename != NULL) ? fopen(pPCMDataSource->pUserData->pcmFileA.filename, "rb") : NULL;
                currentFile = pPCMDataSource->fileA;
            }
            else
            {
                pPCMDataSource->fileB = (pPCMDataSource->pUserData->pcmFileB.filename != NULL) ? fopen(pPCMDataSource->pUserData->pcmFileB.filename, "rb") : NULL;
                currentFile = pPCMDataSource->fileB;
            }

            // Set the new current file and reset any necessary variables
            pPCMDataSource->currentPCMFrame = 0;

            eofReached = true;
            break; // Exit the loop after the file switch
        }  

        // Read from the current file
        FILE *currentFile;
        if (pPCMDataSource->currentFileIndex == 0)
        {
            currentFile = pPCMDataSource->fileA;
        }
        else
        {
            currentFile = pPCMDataSource->fileB;
        }

        ma_uint32 bytesRead = 0;

        if (currentFile != NULL)
            bytesRead = (ma_uint32)fread((char *)pFramesOut + (framesRead * bytesPerFrame), 1, bytesToRead, currentFile);
            
        if (bytesRead == 0 || skipToNext)
        {
            activateSwitch(pPCMDataSource);
            continue; // Continue to the next iteration to read from the new file
        }

        framesRead += bytesRead / bytesPerFrame;
        framesToRead -= bytesRead / bytesPerFrame;
        bytesToRead -= bytesRead;
    }

    // Allocate memory for g_audioBuffer (if not already allocated)
    if (g_audioBuffer == NULL)
    {
        g_audioBuffer = malloc(sizeof(ma_int32) * frameCount);
        if (g_audioBuffer == NULL)
        {
            // Memory allocation failed
            return;
        }
    }

    // No format conversion needed, just copy the audio samples
    memcpy(g_audioBuffer, pFramesOut, sizeof(ma_int32) * framesRead);

    if (pFramesRead != NULL)
        *pFramesRead = framesRead;
}

ma_uint64 pcm_file_data_source_get_pcm_frame_count(ma_data_source *pDataSource)
{
    PCMFileDataSource *pPCMDataSource = (PCMFileDataSource *)pDataSource;

    // Get the current file based on the current file index
    FILE *currentFile;
    if (pPCMDataSource->pUserData->currentFileIndex == 0)
    {
        currentFile = pPCMDataSource->fileA;
    }
    else
    {
        currentFile = pPCMDataSource->fileB;
    }

    // Seek to the end of the current file to get its size
    fseek(currentFile, 0, SEEK_END);
    ma_uint64 fileSize = ftell(currentFile);
    fseek(currentFile, 0, SEEK_SET);

    // Calculate the total number of frames in the current file
    ma_uint64 frameCount = fileSize / ma_get_bytes_per_frame(pPCMDataSource->format, pPCMDataSource->channels);

    return frameCount;
}

// Callback function to retrieve the current read position in PCM frames
ma_uint64 pcm_file_data_source_get_cursor_pcm_frame(ma_data_source *pDataSource)
{
    PCMFileDataSource *pPCMDataSource = (PCMFileDataSource *)pDataSource;

    // Get the current PCM frame position
    ma_uint64 currentFrame = pPCMDataSource->currentPCMFrame;

    return currentFrame;
}

void pcm_file_data_source_seek_pcm_frame(ma_data_source *pDataSource, ma_uint64 frameIndex)
{
    PCMFileDataSource *pPCMDataSource = (PCMFileDataSource *)pDataSource;
    ma_uint64 targetFrame = frameIndex;

    if (targetFrame > pPCMDataSource->currentPCMFrame)
    {
        // Seeking forward
        ma_uint64 framesToSkip = targetFrame - pPCMDataSource->currentPCMFrame;
        ma_uint32 bytesPerFrame = ma_get_bytes_per_frame(pPCMDataSource->format, pPCMDataSource->channels);
        ma_uint32 bytesToSkip = (ma_uint32)(framesToSkip * bytesPerFrame);
        FILE *currentFile;

        if (pPCMDataSource->currentFileIndex == 0)
        {
            currentFile = pPCMDataSource->fileA;
        }
        else
        {
            currentFile = pPCMDataSource->fileB;
        }

        fseek(currentFile, bytesToSkip, SEEK_CUR);
        pPCMDataSource->currentPCMFrame = targetFrame;
    }
    else if (targetFrame < pPCMDataSource->currentPCMFrame)
    {
        // Seeking backward
    }
}

void on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount)
{
    PCMFileDataSource *pDataSource = (PCMFileDataSource *)pDevice->pUserData;
    ma_uint64 framesRead = 0;
    pcm_file_data_source_read_pcm_frames(&pDataSource->base, pFramesOut, frameCount, &framesRead);
    (void)pFramesIn;

    if (framesRead < frameCount && pDataSource->switchFiles)
    {
        pDataSource->switchFiles = false;

        if (pDataSource->currentFileIndex == 0)
        {
            // Switch to file A
            pDataSource->currentFileIndex = 1;
            ma_data_source_seek_to_pcm_frame(&pDataSource->base, 0); // Seek to the beginning of file A
            pcm_file_data_source_read_pcm_frames(&pDataSource->base, (ma_int16 *)pFramesOut + framesRead * pDevice->playback.channels, frameCount - framesRead, &framesRead);
        }
        else if (pDataSource->currentFileIndex == 1)
        {
            // Switch to file B
            pDataSource->currentFileIndex = 0;
            ma_data_source_seek_to_pcm_frame(&pDataSource->base, 0); // Seek to the beginning of file B
            pcm_file_data_source_read_pcm_frames(&pDataSource->base, (ma_int16 *)pFramesOut + framesRead * pDevice->playback.channels, frameCount - framesRead, &framesRead);
        }
    }
}

void cleanupProcess(pid_t *pidToKill) 
{
    if (*pidToKill != -1) {
        kill(*pidToKill, SIGTERM);
        int status;
        waitpid(*pidToKill, &status, 0);
        *pidToKill = -1;
    }
}

int convertToPcmFile(const char *filePath, const char *outputFilePath)
{
    const int COMMAND_SIZE = 1000;
    char command[COMMAND_SIZE];

    char *escapedInputFilePath = escapeFilePath(filePath);

    snprintf(command, sizeof(command),
             "ffmpeg -v fatal -hide_banner -nostdin -y -i \"%s\" -f s24le -acodec pcm_s24le -ac %d -ar %d -threads auto \"%s\"",
             escapedInputFilePath, CHANNELS, SAMPLE_RATE, outputFilePath);

    free(escapedInputFilePath);

    pid_t currentPid = -1;
    pid_t *pidToKill = NULL;

    // alternate between killing two processes that linger, kill the least recent one
    if (usepid2)
    {
        pidToKill = &pid2;
    }
    else {
        pidToKill = &pid;
    }

    cleanupProcess(pidToKill);

    currentPid = fork();
    if (currentPid == -1)
    {
        perror("fork failed");
        return -1;
    }
    else if (currentPid == 0)
    {
        // Child process

        // Detach from the parent process and create a new session
        if (setsid() == -1)
        {
            perror("setsid failed");
            exit(EXIT_FAILURE);
        }

        // Close standard file descriptors
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        // Execute the command using execvp
        char *args[] = {"sh", "-c", command, NULL};
        execvp("sh", args);

        // execvp only returns if an error occurs
        perror("execvp failed");
        exit(EXIT_FAILURE);
    }
    else
    {
        // Parent process
        if (!usepid2)
        {
            pid = currentPid;
            usepid2 = true;
        }
        else
        {
            pid2 = currentPid;
            usepid2 = false;
        }

        // Close the child process handle immediately to prevent lingering processes
        //close(currentPid);
        return 0;
    }
}

void resumePlayback()
{
    if (!ma_device_is_started(&device))
    {
        ma_device_start(&device);
    }
    paused = false;    
}

void pausePlayback()
{

    if (ma_device_is_started(&device))
    {
        ma_device_stop(&device);
        paused = true;
    }
}

void togglePausePlayback()
{

    if (ma_device_is_started(&device))
    {
        ma_device_stop(&device);
        paused = true;
    }
    else if (paused)
    {
        resumePlayback();
        paused = false;
    }
}

bool isPaused()
{
    return paused;
}

bool isPlaybackDone()
{
    if (eofReached)
    {
        eofReached = false;
        return true;
    }
    else
    {
        return false;
    }
}

void stopPlayback()
{
    ma_device_stop(&device);
}

void cleanupPlaybackDevice()
{
    ma_device_stop(&device);
    while (ma_device_get_state(&device) == ma_device_state_started)
    {
        usleep(100000);
    }
    ma_device_uninit(&device);
}

int getSongDuration(const char *filePath, double *duration)
{
    char command[1024];
    FILE *pipe;
    char output[1024];
    char *durationStr;
    double durationValue;

    char *escapedInputFilePath = escapeFilePath(filePath);

    snprintf(command, sizeof(command), "ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 \"%s\"", escapedInputFilePath);

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
    getSongDuration(filepath, &duration);
    return duration;
}

int getCurrentVolume()
{
    FILE *fp;
    char command_str[1000];
    char buf[100];
    int currentVolume = -1;

    // Build the command string
    snprintf(command_str, sizeof(command_str),
             "pactl get-sink-volume @DEFAULT_SINK@ | awk 'NR==1{print $5}'");

    // Execute the command and read the output
    fp = popen(command_str, "r");
    if (fp != NULL) {
        if (fgets(buf, sizeof(buf), fp) != NULL) {
            sscanf(buf, "%d", &currentVolume);
        }
        pclose(fp);
    }

    return currentVolume;
}

void setVolume(int volume)
{
    char command_str[1000];
    FILE *fp;

    // Limit new volume to a maximum of 100%
    if (volume > 100)
    {
        volume = 100;
    }
    else if (volume < 0)
    {
        volume = 0;
    }

    snprintf(command_str, 1000, "pactl set-sink-volume @DEFAULT_SINK@ %d%%", volume);

    fp = popen(command_str, "r");
    if (fp == NULL)
    {
        return;
    }
    pclose(fp);  
}

int adjustVolumePercent(int volumeChange)
{
    char command_str[1000];
    FILE *fp;
    int currentVolume = getCurrentVolume();

    int newVolume = currentVolume + volumeChange;

    // Limit new volume to a maximum of 100%
    if (newVolume > 100)
    {
        newVolume = 100;
    }
    else if (newVolume < 0)
    {
        newVolume = 0;
    }

    snprintf(command_str, 1000, "pactl set-sink-volume @DEFAULT_SINK@ %d%%", newVolume);

    fp = popen(command_str, "r");
    if (fp == NULL)
    {
        return -1;
    }
    pclose(fp);

    return 0;
}

void skip()
{
    skipToNext = true;
    repeatEnabled = false;
}

void createAudioDevice(UserData *userData)
{
    ma_result result = ma_context_init(NULL, 0, NULL, &context);
    if (result != MA_SUCCESS)
    {
        printf("Failed to initialize miniaudio context.\n");
        return;
    }

    pcm_file_data_source_init(&pcmDataSource, userData->pcmFileA.filename, userData);

    pcmDataSource.base.vtable = &pcm_file_data_source_vtable;

    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format = SAMPLE_FORMAT;
    deviceConfig.playback.channels = CHANNELS;
    deviceConfig.sampleRate = SAMPLE_RATE;
    deviceConfig.dataCallback = on_audio_frames;
    deviceConfig.pUserData = &pcmDataSource;

    result = ma_device_init(&context, &deviceConfig, &device);
    if (result != MA_SUCCESS)
    {
        printf("Failed to initialize miniaudio device.\n");
        return;
    }

    result = ma_device_start(&device);
    if (result != MA_SUCCESS)
    {
        printf("Failed to start miniaudio device.\n");
        return;
    }
}
