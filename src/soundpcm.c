#include "soundpcm.h"

/*

soundpcm.c

 Functions related to miniaudio implementation for pcm audio files

*/

#define CHANNELS 2
#define SAMPLE_RATE 192000
#define SAMPLE_WIDTH 3
#define SAMPLE_FORMAT ma_format_s24

void pcm_read_pcm_frames(ma_data_source *pDataSource, void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead)
{
        AudioData *pAudioData = (AudioData *)pDataSource;
        ma_uint32 framesToRead = (ma_uint32)frameCount;

        ma_uint32 bytesPerFrame = ma_get_bytes_per_frame(pAudioData->format, pAudioData->channels);
        ma_uint32 bytesToRead = framesToRead * bytesPerFrame;
        ma_uint32 framesRead = 0;

        while (framesToRead > 0)
        {
                if (isImplSwitchReached())
                        return;

                if (pAudioData == NULL)
                        return;

                if (pthread_mutex_trylock(&dataSourceMutex) != 0)
                {                        
                        return;
                }                        

                if (pAudioData->switchFiles)
                {
                        executeSwitch(pAudioData);
                        pthread_mutex_unlock(&dataSourceMutex);                        
                        break;
                }

                if (getCurrentImplementationType() != PCM && !isSkipToNext())
                {
                        pthread_mutex_unlock(&dataSourceMutex);
                        return;
                }

                FILE *currentFile;
                currentFile = (pAudioData->currentFileIndex == 0) ? pAudioData->fileA : pAudioData->fileB;
                ma_uint32 bytesRead = 0;

                if (isSeekRequested())
                {
                        if (currentFile != NULL)
                        {
                                fseek(currentFile, 0, SEEK_END);
                                ma_uint64 fileSize = ftell(currentFile);
                                pAudioData->totalFrames = fileSize / bytesPerFrame;
                                pAudioData->base.rangeEndInFrames = pAudioData->totalFrames;
                        }
                        ma_uint64 seekPercent = getSeekPercentage();
                        if (seekPercent >= 100.0)
                                seekPercent = 100.0;
                        ma_uint32 targetFrame = (pAudioData->totalFrames * seekPercent) / 100;

                        ma_data_source_seek_to_pcm_frame(pDataSource, targetFrame);

                        setSeekRequested(false); // Reset seek flag
                        pthread_mutex_unlock(&dataSourceMutex);
                        break;
                        framesRead = 0;
                        framesToRead = (ma_uint32)frameCount;
                        bytesToRead = framesToRead * bytesPerFrame;
                }

                if (currentFile != NULL)
                        bytesRead = (ma_uint32)fread((char *)pFramesOut + (framesRead * bytesPerFrame), 1, bytesToRead, currentFile);

                // If file is empty, skip
                if ((bytesRead == 0 || isSkipToNext()) && !isEOFReached())
                {
                        activateSwitch(pAudioData);
                        pthread_mutex_unlock(&dataSourceMutex);
                        continue;
                }

                framesRead += bytesRead / bytesPerFrame;
                framesToRead -= bytesRead / bytesPerFrame;
                bytesToRead -= bytesRead;
                setBufferSize(framesRead);

                pthread_mutex_unlock(&dataSourceMutex);
        }

        ma_int32 *audioBuffer = getAudioBuffer();
        if (audioBuffer == NULL)
        {
                audioBuffer = malloc(sizeof(ma_int32) * MAX_BUFFER_SIZE);
                if (audioBuffer == NULL)
                {
                        return;
                }
        }

        memcpy(audioBuffer, pFramesOut, sizeof(ma_int32) * framesRead);
        setAudioBuffer(audioBuffer);

        if (pFramesRead != NULL)
                *pFramesRead = framesRead;
}

void pcm_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount)
{
        AudioData *pDataSource = (AudioData *)pDevice->pUserData;
        ma_uint64 framesRead = 0;
        pcm_read_pcm_frames(&pDataSource->base, pFramesOut, frameCount, &framesRead);
        (void)pFramesIn;
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
        AudioData *pAudioData = (AudioData *)pDataSource;

        // Calculate the byte index
        ma_uint64 byteIndex = frameIndex * ma_get_bytes_per_frame(pAudioData->format, pAudioData->channels);

        // Find the correct file
        FILE *currentFile;
        if (pAudioData->currentFileIndex == 0)
        {
                currentFile = pAudioData->fileA;
        }
        else
        {
                currentFile = pAudioData->fileB;
        }

        if (currentFile != NULL)
        {
                // Seek to the byte index in the file
                int result = fseek(currentFile, byteIndex, SEEK_SET);

                // Set the current frame to frameIndex
                pAudioData->currentPCMFrame = (ma_uint32)frameIndex;

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
        AudioData *pAudioData = (AudioData *)pDataSource;
        *pFormat = pAudioData->format;
        *pChannels = pAudioData->channels;
        *pSampleRate = pAudioData->sampleRate;
        return MA_SUCCESS;
}

static ma_result pcm_file_data_source_get_cursor(ma_data_source *pDataSource, ma_uint64 *pCursor)
{
        AudioData *pAudioData = (AudioData *)pDataSource;
        *pCursor = pAudioData->currentPCMFrame;

        return MA_SUCCESS;
}

static ma_result pcm_file_data_source_get_length(ma_data_source *pDataSource, ma_uint64 *pLength)
{
        AudioData *pAudioData = (AudioData *)pDataSource;

        // Get the current file based on the current file index
        FILE *currentFile;
        if (pAudioData->currentFileIndex == 0)
        {
                currentFile = pAudioData->fileA;
        }
        else
        {
                currentFile = pAudioData->fileB;
        }

        ma_uint64 fileSize;

        // Seek to the end of the current file to get its size
        fseek(currentFile, 0, SEEK_END);
        fileSize = ftell(currentFile);
        fseek(currentFile, 0, SEEK_SET);

        // Calculate the total number of frames in the current file
        ma_uint64 frameCount = fileSize / ma_get_bytes_per_frame(pAudioData->format, pAudioData->channels);
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

ma_data_source_vtable pcm_file_data_source_vtable = {
    pcm_file_data_source_read,
    pcm_file_data_source_seek,
    pcm_file_data_source_get_data_format,
    pcm_file_data_source_get_cursor,
    pcm_file_data_source_get_length,
    pcm_file_data_source_set_looping,
    0 // flags
};
