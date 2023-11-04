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

ma_int32 *g_audioBuffer = NULL;
ma_device device = {0};
ma_context context;
ma_device_config deviceConfig;
PCMFileDataSource pcmDataSource;
bool paused = false;
bool skipToNext = false;
bool repeatEnabled = false;
bool shuffleEnabled = false;
double seekElapsed;

_Atomic bool EOFReached = false;

bool isEOFReached()
{
        return atomic_load(&EOFReached) ? true : false;
}

void setEOFReached()
{
        atomic_store(&EOFReached, true);
}

void setEOFNotReached()
{
        atomic_store(&EOFReached, false);
}

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
    // Cast to the correct data source type
    PCMFileDataSource *pPCMDataSource = (PCMFileDataSource *)pDataSource;
    
    // Calculate the byte index
    ma_uint64 byteIndex = frameIndex * ma_get_bytes_per_frame(pPCMDataSource->format, pPCMDataSource->channels);

    // Find the correct file
    FILE *currentFile;
    if (pPCMDataSource->currentFileIndex == 0) 
    {
        currentFile = pPCMDataSource->fileA;
    } 
    else 
    {
        currentFile = pPCMDataSource->fileB;
    }

    if (currentFile != NULL) 
    {
        // Seek to the byte index in the file
        int result = fseek(currentFile, byteIndex, SEEK_SET);
      
        // Set the current frame to frameIndex
        pPCMDataSource->currentPCMFrame = (ma_uint32)frameIndex;

        // Check for errors
        if (result == 0) 
        {
            return MA_SUCCESS;
        } 
        else 
        {
            return MA_ERROR;
        }
    } 
    else 
    {
        return MA_ERROR;
    }
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
        if (pPCMDataSource->currentFileIndex == 0)
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

        return MA_SUCCESS;
}

void activateSwitch(PCMFileDataSource *pPCMDataSource)
{
        skipToNext = false;
        if (!repeatEnabled)
                pPCMDataSource->currentFileIndex = 1 - pPCMDataSource->currentFileIndex; // Toggle between 0 and 1
        pPCMDataSource->switchFiles = true;
}

void executeSwitch(PCMFileDataSource *pPCMDataSource)
{
        pPCMDataSource->switchFiles = false;
        
        // Close the current file, and open the new one
        FILE *currentFile;
        if (pPCMDataSource->currentFileIndex == 0)
        {
                currentFile = pPCMDataSource->fileB;
                if (currentFile != NULL)
                        fclose(currentFile);
                pPCMDataSource->fileA = (pPCMDataSource->pUserData->filenameA != NULL) ? 
                fopen(pPCMDataSource->pUserData->filenameA, "rb") : NULL;
                pPCMDataSource->pUserData->currentSongData = pPCMDataSource->pUserData->songdataA;
        }
        else
        {
                currentFile = pPCMDataSource->fileA;
                if (currentFile != NULL)
                        fclose(currentFile);
                pPCMDataSource->fileB = (pPCMDataSource->pUserData->filenameB != NULL) ? 
                fopen(pPCMDataSource->pUserData->filenameB, "rb") : NULL;
                pPCMDataSource->pUserData->currentSongData = pPCMDataSource->pUserData->songdataB;
        }
                                
        pPCMDataSource->currentPCMFrame = 0;

        setEOFReached();

        seekElapsed = 0;        
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
                        executeSwitch(pPCMDataSource);
                        break; // Exit the loop after the file switch
                }

                // Read from the current file
                FILE *currentFile;
                currentFile = (pPCMDataSource->currentFileIndex == 0) ? pPCMDataSource->fileA : pPCMDataSource->fileB;
                ma_uint32 bytesRead = 0;

                if (pPCMDataSource->seekRequested == true)
                {
                        if (currentFile != NULL)
                        {
                                fseek(currentFile, 0, SEEK_END);
                                ma_uint64 fileSize = ftell(currentFile);
                                pPCMDataSource->totalFrames = fileSize / bytesPerFrame;
                                pPCMDataSource->base.rangeEndInFrames = pPCMDataSource->totalFrames;
                        }  

                        ma_uint32 targetFrame = (pPCMDataSource->totalFrames * pPCMDataSource->seekPercentage) / 100;
                        
                        // Set the read pointer for the data source
                        ma_data_source_seek_to_pcm_frame(pDataSource, targetFrame);

                        pPCMDataSource->seekRequested = false; // Reset seek flag                         
                        break;
                        framesRead = 0;  // Reset framesRead
                        framesToRead = (ma_uint32)frameCount;  // Reset framesToRead
                        bytesToRead = framesToRead * bytesPerFrame;  // Reset bytesToRead
                }           

                if (currentFile != NULL)
                        bytesRead = (ma_uint32)fread((char *)pFramesOut + (framesRead * bytesPerFrame), 1, bytesToRead, currentFile);

                // If file is empty, skip
                if ((bytesRead == 0 || skipToNext) && !isEOFReached())
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

void on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount)
{
        PCMFileDataSource *pDataSource = (PCMFileDataSource *)pDevice->pUserData;
        ma_uint64 framesRead = 0;
        pcm_file_data_source_read_pcm_frames(&pDataSource->base, pFramesOut, frameCount, &framesRead);
        (void)pFramesIn;
}

void createAudioDevice(UserData *userData)
{
        ma_result result = ma_context_init(NULL, 0, NULL, &context);
        if (result != MA_SUCCESS)
        {
                printf("Failed to initialize miniaudio context.\n");
                return;
        }

        pcm_file_data_source_init(&pcmDataSource, userData->filenameA, userData);

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

void seekPercentage(float percent)
{
        pcmDataSource.seekPercentage = percent;
        pcmDataSource.seekRequested = true;
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
        if (atomic_load(&EOFReached))
        {
                setEOFNotReached();
                return true;
        }
        else
        {
                return false;
        }
}

void cleanupPlaybackDevice()
{
        if (&device)
        {
                ma_device_stop(&device);
                while (ma_device_get_state(&device) == ma_device_state_started)
                {
                        c_sleep(100);
                }
                ma_device_uninit(&device);
        }
}

void freeAudioBuffer()
{
        if (g_audioBuffer != NULL)
        {
                free(g_audioBuffer);
                g_audioBuffer = NULL;
        }
}

void skip()
{
        skipToNext = true;
        repeatEnabled = false;
}
