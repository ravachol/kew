#define MA_EXPERIMENTAL__DATA_LOOPING_AND_CHAINING
#define MA_NO_ENGINE
#define MINIAUDIO_IMPLEMENTATION
#include "../include/miniaudio/miniaudio.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include "file.h"
#include "soundgapless.h"

#define CHANNELS 2
#define SAMPLE_RATE 44100
#define SAMPLE_WIDTH 2
#define SAMPLE_FORMAT ma_format_s16
#define FRAMES_PER_BUFFER 1024
#define MAX_DATA_SOURCES 10000
#define FRAMES_PER_BUFFER 1024

ma_int16 *g_audioBuffer = NULL;
ma_device device = {0};
ma_context context;
ma_device_config deviceConfig;
PCMFileDataSource pcmDataSource;
PCMFile* pFirstFile = NULL;
bool paused = false;
bool skipToNext = false;
bool repeatEnabled = false;
static int eofReached = 0;


static ma_result pcm_file_data_source_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    PCMFileDataSource* pPCMDataSource = (PCMFileDataSource*)pDataSource;
    ma_uint64 framesToRead = frameCount;
    if (framesToRead > pPCMDataSource->pcmDataSize / ma_get_bytes_per_frame(pPCMDataSource->format, pPCMDataSource->channels)) {
        framesToRead = pPCMDataSource->pcmDataSize / ma_get_bytes_per_frame(pPCMDataSource->format, pPCMDataSource->channels);
    }
    
    memcpy(pFramesOut, pPCMDataSource->pcmData, framesToRead * ma_get_bytes_per_frame(pPCMDataSource->format, pPCMDataSource->channels));
    *pFramesRead = framesToRead;
    
    pPCMDataSource->pcmData += framesToRead * ma_get_bytes_per_frame(pPCMDataSource->format, pPCMDataSource->channels);
    pPCMDataSource->pcmDataSize -= framesToRead * ma_get_bytes_per_frame(pPCMDataSource->format, pPCMDataSource->channels);

    return MA_SUCCESS;
}

static ma_result pcm_file_data_source_seek(ma_data_source* pDataSource, ma_uint64 frameIndex)
{
    // Not implemented for this example.
    return MA_SUCCESS;
}

static ma_result pcm_file_data_source_get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
{
    PCMFileDataSource* pPCMDataSource = (PCMFileDataSource*)pDataSource;
    *pFormat = pPCMDataSource->format;
    *pChannels = pPCMDataSource->channels;
    *pSampleRate = pPCMDataSource->sampleRate;

    // If channelMap is not NULL and channelMapCap is large enough, you can set the channel map here.

    return MA_SUCCESS;
}

static ma_result pcm_file_data_source_get_cursor(ma_data_source* pDataSource, ma_uint64* pCursor)
{
    // Not implemented for this example.
    return MA_SUCCESS;
}

static ma_result pcm_file_data_source_get_length(ma_data_source* pDataSource, ma_uint64* pLength)
{
    PCMFileDataSource* pPCMDataSource = (PCMFileDataSource*)pDataSource;
    *pLength = pPCMDataSource->pcmDataSize / ma_get_bytes_per_frame(pPCMDataSource->format, pPCMDataSource->channels);

    return MA_SUCCESS;
}

static ma_data_source_vtable pcm_file_data_source_vtable = {
    pcm_file_data_source_read,
    pcm_file_data_source_seek,
    pcm_file_data_source_get_data_format,
    pcm_file_data_source_get_cursor,
    pcm_file_data_source_get_length
};

ma_result pcm_file_data_source_init(PCMFileDataSource* pPCMDataSource, const char* filename, char* pcmData, ma_uint64 pcmDataSize, ma_format format, ma_uint32 channels, ma_uint32 sampleRate, UserData* pUserData)
{
    pPCMDataSource->pUserData = pUserData;
    pPCMDataSource->filename = filename;
    pPCMDataSource->pcmData = pcmData;
    pPCMDataSource->pcmDataSize = pcmDataSize;
    pPCMDataSource->format = format;
    pPCMDataSource->channels = channels;
    pPCMDataSource->sampleRate = sampleRate;
    pPCMDataSource->currentPCMFrame = 0;

    return MA_SUCCESS;
}

void pcm_file_data_source_read_pcm_frames(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    PCMFileDataSource* pPCMDataSource = (PCMFileDataSource*)pDataSource;
    UserData* pUserData = pPCMDataSource->pUserData;  // Access the UserData pointer

    // End of file
    if (skipToNext || (pPCMDataSource->currentPCMFrame >= pPCMDataSource->pcmDataSize / ma_get_bytes_per_frame(pPCMDataSource->format, pPCMDataSource->channels)))
    {
        eofReached = 1;
        skipToNext = false;

        if (pUserData->pcmFileA.filename != NULL || pUserData->pcmFileB.filename != NULL)
        {
            if (!repeatEnabled)            
                pUserData->currentFileIndex++;             

            if (pUserData->currentFileIndex % 2 == 0)
            {
                // Even index, load pcmFileA
                PCMFile* pNextFile = &pUserData->pcmFileA;
                if (pNextFile->filename != NULL)
                {
                    pcm_file_data_source_init(pPCMDataSource, pNextFile->filename, pNextFile->pcmData, pNextFile->pcmDataSize, pPCMDataSource->format, pPCMDataSource->channels, pPCMDataSource->sampleRate, pUserData);
                }
                else
                {
                    // pcmFileA is null, reached the end
                    pUserData->endOfListReached = 1;
                    if (pFramesRead != NULL) {
                        *pFramesRead = 0;
                    }
                    return;
                }
            }
            else
            {
                // Odd index, load pcmFileB
                PCMFile* pNextFile = &pUserData->pcmFileB;
                if (pNextFile->filename != NULL)
                {
                    pcm_file_data_source_init(pPCMDataSource, pNextFile->filename, pNextFile->pcmData, pNextFile->pcmDataSize, pPCMDataSource->format, pPCMDataSource->channels, pPCMDataSource->sampleRate, pUserData);
                }
                else
                {
                    // pcmFileB is null, reached the end
                    pUserData->endOfListReached = 1;
                    if (pFramesRead != NULL) {
                        *pFramesRead = 0;
                    }
                    return;
                }
            }
        }
        else
        {
            // Both pcmFileA and pcmFileB are null, reached the end
            pUserData->endOfListReached = 1;
            if (pFramesRead != NULL) {
                *pFramesRead = 0;
            }
            return;
        }
    }
    
    // Calculate the number of frames to read
    ma_uint32 framesRemaining = (ma_uint32)(pPCMDataSource->pcmDataSize / ma_get_bytes_per_frame(pPCMDataSource->format, pPCMDataSource->channels)) - pPCMDataSource->currentPCMFrame;
    ma_uint32 framesToRead = (ma_uint32)ma_min(frameCount, framesRemaining);
    
    // Copy PCM data to the output buffer
    ma_uint64 bytesToRead = framesToRead * ma_get_bytes_per_frame(pPCMDataSource->format, pPCMDataSource->channels);
    memcpy(pFramesOut, pPCMDataSource->pcmData + (pPCMDataSource->currentPCMFrame * ma_get_bytes_per_frame(pPCMDataSource->format, pPCMDataSource->channels)), (size_t)bytesToRead);
    
    // Allocate memory for g_audioBuffer (if not already allocated)
    if (g_audioBuffer == NULL)
    {
        g_audioBuffer = malloc(sizeof(ma_int16) * frameCount);
        if (g_audioBuffer == NULL)
        {
            // Memory allocation failed
            return;
        }
    }

    // Copy the audio samples from pOutput to audioBuffer
    memcpy(g_audioBuffer, pFramesOut, sizeof(ma_int16) * frameCount);

    // Update the current PCM frame position and set the number of frames read
    pPCMDataSource->currentPCMFrame += framesToRead;
    if (pFramesRead != NULL) {
        *pFramesRead = framesToRead;
    }
}

ma_result pcm_file_data_source_seek_pcm_frame(ma_data_source* pDataSource, ma_uint64 frameIndex)
{
    PCMFileDataSource* pPCMDataSource = (PCMFileDataSource*)pDataSource;
    UserData* pUserData = pPCMDataSource->pUserData; 

    // Calculate the maximum valid frame index in the current file
    ma_uint64 maxFrameIndex = pPCMDataSource->pcmDataSize / ma_get_bytes_per_frame(pPCMDataSource->format, pPCMDataSource->channels);

    if (frameIndex < maxFrameIndex)
    {
        // Seeking within the current file
        pPCMDataSource->currentPCMFrame = (ma_uint32)frameIndex;
    }
    else
    {
        // Seeking to a different file
        ma_uint32 fileIndex = (ma_uint32)(frameIndex / maxFrameIndex);

        // Move to the specified file
        if (fileIndex == 0)
        {
            pUserData->currentFileIndex = 0;
            PCMFile* pNextFile = &pUserData->pcmFileA;
            pcm_file_data_source_init(pPCMDataSource, pNextFile->filename, pNextFile->pcmData, pNextFile->pcmDataSize, SAMPLE_FORMAT, CHANNELS, SAMPLE_RATE, pUserData);
        }
        else if (fileIndex == 1)
        {
            pUserData->currentFileIndex = 1;
            PCMFile* pNextFile = &pUserData->pcmFileB;
            pcm_file_data_source_init(pPCMDataSource, pNextFile->filename, pNextFile->pcmData, pNextFile->pcmDataSize, SAMPLE_FORMAT, CHANNELS, SAMPLE_RATE, pUserData);
        }
        else
        {
            // Handle the case where end of list is reached
            pUserData->endOfListReached = 1;
            return MA_INVALID_OPERATION;
        }

        // Update the current PCM frame position within the new file
        pPCMDataSource->currentPCMFrame = (ma_uint32)(frameIndex % maxFrameIndex);
    }

    return MA_SUCCESS;
}

// Callback function to retrieve the total number of PCM frames in the file
ma_uint64 pcm_file_data_source_get_pcm_frame_count(ma_data_source* pDataSource)
{
    PCMFileDataSource* pPCMDataSource = (PCMFileDataSource*)pDataSource;
    
    // Calculate the total number of frames
    ma_uint64 frameCount = pPCMDataSource->pcmDataSize / ma_get_bytes_per_frame(pPCMDataSource->format, pPCMDataSource->channels);
    
    return frameCount;
}

// Callback function to retrieve the current read position in PCM frames
ma_uint64 pcm_file_data_source_get_cursor_pcm_frame(ma_data_source* pDataSource)
{
    PCMFileDataSource* pPCMDataSource = (PCMFileDataSource*)pDataSource;
    
    // Get the current PCM frame position
    ma_uint64 currentFrame = pPCMDataSource->currentPCMFrame;
    
    return currentFrame;
}

void cleanup_pcm_files(PCMFile* pcmFiles, ma_uint32 pcmFileCount)
{
    for (ma_uint32 i = 0; i < pcmFileCount; ++i) // FIXME NEEDS TO GET THE COUNT FROM THE ACTUAL ARRAY
    {
        free(pcmFiles[i].pcmData);
    }

    free(pcmFiles);
}

// Callback that will be called by miniaudio whenever new frames are available
void on_audio_frames(ma_device* pDevice, void* pFramesOut, const void* pFramesIn, ma_uint32 frameCount)
{
    PCMFileDataSource* pDataSource = (PCMFileDataSource*)pDevice->pUserData;
    ma_uint64 framesRead = 0;
    pcm_file_data_source_read_pcm_frames(pDataSource, pFramesOut, frameCount, &framesRead);
}

int convertToPcmFile(const char *filePath, const char *outputFilePath)
{
    const int COMMAND_SIZE = 1000;
    char command[COMMAND_SIZE];
    int status;

    const char* escapedInputFilePath = escapeFilePath(filePath);
    // Construct the command string
    snprintf(command, sizeof(command),
             "ffmpeg -v fatal -hide_banner -nostdin -y -i \"%s\" -f s16le -acodec pcm_s16le -ac %d -ar %d \"%s\"",
             escapedInputFilePath, CHANNELS, SAMPLE_RATE, outputFilePath);
    // Execute the command
    pid_t pid = fork();

    if (pid == -1) {
        // Fork failed
        perror("fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        execl("/bin/sh", "sh", "-c", command, (char *)NULL);
        exit(EXIT_SUCCESS);
    } else {
        // Parent process
        waitpid(pid, &status, 0);  // Wait for the chafa process to finish
    }
    return 0;
}

void resumePlayback()
{
    if (!ma_device_is_started(&device))
    {
        ma_device_start(&device);
    }
}

void pausePlayback()
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

int isPlaybackDone()
{
    if (eofReached == 1)
    {
        eofReached = 0;
        return 1;
    }
    else
    {
        return 0;
    }
}

void stopPlayback()
{
    ma_device_stop(&device);
}

void cleanupPlaybackDevice()
{
    ma_device_stop(&device);
    while (ma_device_get_state(&device) == ma_device_state_started) {
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

    const char* escapedInputFilePath = escapeFilePath(filePath);

    snprintf(command, sizeof(command), "ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 \"%s\"", escapedInputFilePath);

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

int adjustVolumePercent(int volumeChange)
{
    char command_str[1000];
    if (volumeChange > 0)
        snprintf(command_str, 1000, "amixer -D pulse sset Master %d%%+", volumeChange);
    else
        snprintf(command_str, 1000, "amixer -D pulse sset Master %d%%-", -volumeChange);
    FILE *fp = popen(command_str, "r");
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
}

void loadPcmFile(PCMFile* pcmFile, const char* filename)
{
    FILE* file = fopen(filename, "rb");
    if (file == NULL)
    {
        //printf("Failed to open PCM file: %s\n", filename);
        return;
    }

    // Get the file size
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    rewind(file);

    // Allocate memory for PCM data
    char* pcmData = (char*)malloc(fileSize);
    if (pcmData == NULL)
    {
        printf("Failed to allocate memory for PCM data.\n");
        fclose(file);
        return;
    }

    // Read PCM data from the file
    if (fread(pcmData, 1, fileSize, file) != (size_t)fileSize)
    {
        printf("Failed to read PCM data from file: %s\n", filename);
        fclose(file);
        free(pcmData);
        return;
    }

    fclose(file);

    // Store PCM file information in the pcmFile struct
    pcmFile->filename = filename;
    pcmFile->pcmData = pcmData;
    pcmFile->pcmDataSize = fileSize;
}

void createAudioDevice(UserData *userData)
{
    ma_result result = ma_context_init(NULL, 0, NULL, &context);

    pFirstFile = &userData->pcmFileA;
    pcm_file_data_source_init(&pcmDataSource, pFirstFile->filename, pFirstFile->pcmData, pFirstFile->pcmDataSize, SAMPLE_FORMAT, CHANNELS, SAMPLE_RATE, userData);
    pcmDataSource.base.vtable = &pcm_file_data_source_vtable;

    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format = SAMPLE_FORMAT;
    deviceConfig.playback.channels = CHANNELS;
    deviceConfig.sampleRate = SAMPLE_RATE;
    deviceConfig.dataCallback = on_audio_frames;
    deviceConfig.pUserData = &pcmDataSource;

    result = ma_device_init(&context, &deviceConfig, &device);
    if (result != MA_SUCCESS) {
    }

    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
    }  
}