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
        PCMFileDataSource *pPCMDataSource = (PCMFileDataSource *)pDataSource;
        ma_uint32 framesToRead = (ma_uint32)frameCount;

        ma_uint32 bytesPerFrame = ma_get_bytes_per_frame(pPCMDataSource->format, pPCMDataSource->channels);
        ma_uint32 bytesToRead = framesToRead * bytesPerFrame;
        ma_uint32 framesRead = 0;

        while (framesToRead > 0)
        {
                if (pPCMDataSource == NULL)
                        return;

                if (pPCMDataSource->switchFiles)
                {
                        executeSwitch(pPCMDataSource);
                        break;
                }

                if (getCurrentImplementationType() != PCM && !isSkipToNext())
                        return;

                FILE *currentFile;
                currentFile = (pPCMDataSource->currentFileIndex == 0) ? pPCMDataSource->fileA : pPCMDataSource->fileB;
                ma_uint32 bytesRead = 0;

                if (isSeekRequested())
                {
                        if (currentFile != NULL)
                        {
                                fseek(currentFile, 0, SEEK_END);
                                ma_uint64 fileSize = ftell(currentFile);
                                pPCMDataSource->totalFrames = fileSize / bytesPerFrame;
                                pPCMDataSource->base.rangeEndInFrames = pPCMDataSource->totalFrames;
                        }

                        ma_uint32 targetFrame = (pPCMDataSource->totalFrames * getSeekPercentage()) / 100;

                        ma_data_source_seek_to_pcm_frame(pDataSource, targetFrame);

                        setSeekRequested(false); // Reset seek flag
                        break;
                        framesRead = 0;
                        framesToRead = (ma_uint32)frameCount;
                        bytesToRead = framesToRead * bytesPerFrame;
                }

                if (currentFile != NULL)
                        bytesRead = (ma_uint32)fread((char *)pFramesOut + (framesRead * bytesPerFrame), 1, bytesToRead, currentFile);
                else if (pPCMDataSource->pUserData->currentSongData == NULL ||
                         hasBuiltinDecoder(pPCMDataSource->pUserData->currentSongData->filePath) ||
                         (endsWith(pPCMDataSource->pUserData->currentSongData->filePath, "opus")) ||
                         (endsWith(pPCMDataSource->pUserData->currentSongData->filePath, "ogg")))
                        return;

                // If file is empty, skip
                if ((bytesRead == 0 || isSkipToNext()) && !isEOFReached())
                {
                        activateSwitch(pPCMDataSource);
                        continue;
                }

                framesRead += bytesRead / bytesPerFrame;
                framesToRead -= bytesRead / bytesPerFrame;
                bytesToRead -= bytesRead;
                setBufferSize(framesRead);
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
        PCMFileDataSource *pDataSource = (PCMFileDataSource *)pDevice->pUserData;
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

ma_data_source_vtable pcm_file_data_source_vtable = {
    pcm_file_data_source_read,
    pcm_file_data_source_seek,
    pcm_file_data_source_get_data_format,
    pcm_file_data_source_get_cursor,
    pcm_file_data_source_get_length,
    pcm_file_data_source_set_looping,
    0 // flags
};