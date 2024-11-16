#include "soundbuiltin.h"
/*

soundbuiltin.c

 Functions related to miniaudio implementation for miniaudio built-in decoders (flac, wav and mp3)

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
        AudioData *audioData = (AudioData *)pDataSource;

        if (getCurrentBuiltinDecoder() == NULL)
        {
                return MA_INVALID_ARGS;
        }

        ma_result result = ma_decoder_seek_to_pcm_frame(getCurrentBuiltinDecoder(), frameIndex);

        if (result == MA_SUCCESS)
        {
                audioData->currentPCMFrame = (ma_uint32)frameIndex;
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
        AudioData *audioData = (AudioData *)pDataSource;
        *pFormat = audioData->format;
        *pChannels = audioData->channels;
        *pSampleRate = audioData->sampleRate;
        return MA_SUCCESS;
}

static ma_result builtin_file_data_source_get_cursor(ma_data_source *pDataSource, ma_uint64 *pCursor)
{
        AudioData *audioData = (AudioData *)pDataSource;
        *pCursor = audioData->currentPCMFrame;

        return MA_SUCCESS;
}

static ma_result builtin_file_data_source_get_length(ma_data_source *pDataSource, ma_uint64 *pLength)
{
        (void)pDataSource;
        ma_uint64 totalFrames = 0;

        if (getCurrentBuiltinDecoder() == NULL)
        {
                return MA_INVALID_ARGS;
        }

        ma_result result = ma_decoder_get_length_in_pcm_frames(getCurrentBuiltinDecoder(), &totalFrames);

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
        AudioData *audioData = (AudioData *)pDataSource;
        ma_uint64 framesRead = 0;

        while (framesRead < frameCount)
        {
                ma_uint64 remainingFrames = frameCount - framesRead;

                if (pthread_mutex_trylock(&dataSourceMutex) != 0)
                {
                        return;
                }

                if (isImplSwitchReached() || audioData == NULL)
                {
                        pthread_mutex_unlock(&dataSourceMutex);
                        return;
                }

                if (audioData->switchFiles)
                {
                        executeSwitch(audioData);
                        pthread_mutex_unlock(&dataSourceMutex);
                        break;
                }

                ma_decoder *decoder = getCurrentBuiltinDecoder();

                if ((getCurrentImplementationType() != BUILTIN && !isSkipToNext()))
                {
                        pthread_mutex_unlock(&dataSourceMutex);                        
                        return;
                }

                if (audioData->totalFrames == 0)
                        ma_data_source_get_length_in_pcm_frames(decoder, &audioData->totalFrames);

                if (isSeekRequested())
                {
                        ma_uint64 totalFrames = audioData->totalFrames;
                        ma_uint64 seekPercent = getSeekPercentage();
                        if (seekPercent >= 100.0)
                                seekPercent = 100.0;
                        ma_uint64 targetFrame = (ma_uint64)((totalFrames - 1) * seekPercent / 100.0);

                        if (targetFrame >= totalFrames)
                                targetFrame = totalFrames - 1;

                        ma_result seekResult = ma_decoder_seek_to_pcm_frame(decoder, targetFrame);

                        if (seekResult != MA_SUCCESS)
                        {
                                setSeekRequested(false);
                                pthread_mutex_unlock(&dataSourceMutex);
                                return;
                        }

                        setSeekRequested(false);
                }

                ma_uint64 framesToRead = 0;
                ma_decoder *firstDecoder = getFirstDecoder();
                ma_uint64 cursor = 0;
                ma_result result;

                if (firstDecoder == NULL)
                {                       
                        pthread_mutex_unlock(&dataSourceMutex);
                        return;
                }

                if (isEOFReached())
                {
                        pthread_mutex_unlock(&dataSourceMutex);
                        return;
                }
                
                result = ma_data_source_read_pcm_frames(firstDecoder, (ma_int32 *)pFramesOut + framesRead * audioData->channels, remainingFrames, &framesToRead);
                ma_data_source_get_cursor_in_pcm_frames(decoder, &cursor);

                if (((audioData->totalFrames != 0 && cursor != 0 && cursor >= audioData->totalFrames) || framesToRead == 0 || isSkipToNext() || result != MA_SUCCESS) && !isEOFReached())
                {
                        activateSwitch(audioData);
                        pthread_mutex_unlock(&dataSourceMutex);
                        continue;
                }

                framesRead += framesToRead;
                setBufferSize(framesToRead);

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
        {
                *pFramesRead = framesRead;
        }
}

void builtin_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount)
{
        AudioData *pDataSource = (AudioData *)pDevice->pUserData;
        ma_uint64 framesRead = 0;
        builtin_read_pcm_frames(&pDataSource->base, pFramesOut, frameCount, &framesRead);
        (void)pFramesIn;
}
