#include "soundbuiltin.h"
/*

soundbuiltin.c

 Functions related to miniaudio implementation for miniaudio builtin decoders (flac, wav and mp3)

*/

static ma_result builtin_file_data_source_read(ma_data_source *pDataSource, void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead)
{
        // Dummy implementation
        (void)pDataSource;
        (void)pFramesOut;
        (void)frameCount;
        (void)pFramesRead;
        return MA_SUCCESS;
}

static ma_result builtin_file_data_source_seek(ma_data_source *pDataSource, ma_uint64 frameIndex)
{
    PCMFileDataSource* pPCMDataSource = (PCMFileDataSource*)pDataSource;

    if (getCurrentDecoder() == NULL)
    {
        return MA_INVALID_ARGS;
    }

    ma_result result = ma_decoder_seek_to_pcm_frame(getCurrentDecoder(), frameIndex);
    
    if (result == MA_SUCCESS)
    {
        pPCMDataSource->currentPCMFrame = (ma_uint32)frameIndex;
        return MA_SUCCESS;
    }
    else
    {
        return result;
    }
}

static ma_result builtin_file_data_source_get_data_format(ma_data_source *pDataSource, ma_format *pFormat, ma_uint32 *pChannels, ma_uint32 *pSampleRate, ma_channel *pChannelMap, size_t channelMapCap)
{
        (void)pChannelMap;
        (void)channelMapCap;
        PCMFileDataSource *pPCMDataSource = (PCMFileDataSource *)pDataSource;
        *pFormat = pPCMDataSource->format;
        *pChannels = pPCMDataSource->channels;
        *pSampleRate = pPCMDataSource->sampleRate;
        return MA_SUCCESS;
}

static ma_result builtin_file_data_source_get_cursor(ma_data_source *pDataSource, ma_uint64 *pCursor)
{
        PCMFileDataSource *pPCMDataSource = (PCMFileDataSource *)pDataSource;
        *pCursor = pPCMDataSource->currentPCMFrame;

        return MA_SUCCESS;
}

static ma_result builtin_file_data_source_get_length(ma_data_source *pDataSource, ma_uint64 *pLength)
{
    ma_uint64 totalFrames = 0;

    if (getCurrentDecoder() == NULL)
    {
        return MA_INVALID_ARGS;
    }

    ma_result result = ma_decoder_get_length_in_pcm_frames(getCurrentDecoder(), &totalFrames);

    if (result != MA_SUCCESS)
    {
        return result;
    }

    *pLength = totalFrames;

    return MA_SUCCESS;
}

static ma_result builtin_file_data_source_set_looping(ma_data_source *pDataSource, ma_bool32 isLooping)
{
        // Dummy implementation
        (void)pDataSource;
        (void)isLooping;

        return MA_SUCCESS;
}

ma_data_source_vtable builtin_file_data_source_vtable = {
    builtin_file_data_source_read,
    builtin_file_data_source_seek,
    builtin_file_data_source_get_data_format,
    builtin_file_data_source_get_cursor,
    builtin_file_data_source_get_length,
    builtin_file_data_source_set_looping,
    0 // flags
};

void builtin_read_pcm_frames(ma_data_source *pDataSource, void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead)
{
        PCMFileDataSource *pPCMDataSource = (PCMFileDataSource *)pDataSource;
        ma_uint64 framesRead = 0;

        while (framesRead < frameCount)
        {
                ma_uint64 remainingFrames = frameCount - framesRead; 

                if (isImplSwitchReached())
                        return;                               

                if (pPCMDataSource == NULL)
                        return;

                if (pPCMDataSource->switchFiles)
                {
                        executeSwitch(pPCMDataSource);
                        break;
                }

                if (getCurrentImplementationType() != BUILTIN && !isSkipToNext())
                        return;

                ma_decoder *decoder = getCurrentDecoder();

                if (decoder == NULL)
                        return;

                if (pPCMDataSource->totalFrames == 0)
                        ma_data_source_get_length_in_pcm_frames(decoder, &pPCMDataSource->totalFrames);

                if (isSeekRequested())
                {
                        ma_uint64 totalFrames = pPCMDataSource->totalFrames;
                        ma_uint64 seekPercent = getSeekPercentage();
                        if (seekPercent >= 100.0)
                                seekPercent = 100.0;                           
                        ma_uint64 targetFrame = (totalFrames * seekPercent) / 100;
                        ma_result seekResult = ma_decoder_seek_to_pcm_frame(decoder, targetFrame);

                        if (seekResult != MA_SUCCESS)
                        {
                                setSeekRequested(false);
                                return;
                        }

                        setSeekRequested(false);
                }

                ma_uint64 framesToRead = 0;
                ma_decoder *firstDecoder = getFirstDecoder();

                if (decoder == NULL || firstDecoder == NULL)
                        return;

                ma_result result = ma_data_source_read_pcm_frames(firstDecoder, (ma_int32 *)pFramesOut + framesRead * decoder->outputChannels, remainingFrames, &framesToRead);

                ma_uint64 cursor;                
                result = ma_data_source_get_cursor_in_pcm_frames(decoder, &cursor);

                if (((pPCMDataSource->totalFrames != 0 && cursor != 0 && cursor >= pPCMDataSource->totalFrames) || framesToRead == 0 || isSkipToNext() || result != MA_SUCCESS) && !isEOFReached())
                {
                        activateSwitch(pPCMDataSource);
                        continue;
                }

                framesRead += framesToRead;
                setBufferSize(framesToRead);
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
        {
                *pFramesRead = framesRead;
        }
}

void builtin_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount)
{
        PCMFileDataSource *pDataSource = (PCMFileDataSource *)pDevice->pUserData;
        ma_uint64 framesRead = 0;
        builtin_read_pcm_frames(&pDataSource->base, pFramesOut, frameCount, &framesRead);
        (void)pFramesIn;
}