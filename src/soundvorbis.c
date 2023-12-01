#include "soundvorbis.h"
/*

soundvorbis.c

 Functions related to miniaudio implementation for miniaudio vorbis decoding

*/

ma_libvorbis *prevDecoder;

MA_API ma_result ma_libvorbis_read_pcm_frames_wrapper(void *pDecoder, void *pFramesOut, long unsigned int frameCount, long unsigned int *pFramesRead)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_libvorbis_read_pcm_frames((ma_libvorbis *)dec->pUserData, pFramesOut, frameCount, (ma_uint64 *)pFramesRead);
}

MA_API ma_result ma_libvorbis_seek_to_pcm_frame_wrapper(void *pDecoder, long long int frameIndex, ma_seek_origin origin)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_libvorbis_seek_to_pcm_frame((ma_libvorbis *)dec->pUserData, frameIndex);
}

MA_API ma_result ma_libvorbis_get_cursor_in_pcm_frames_wrapper(void *pDecoder, long long int *pCursor)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_libvorbis_get_cursor_in_pcm_frames((ma_libvorbis *)dec->pUserData, (ma_uint64 *)pCursor);
}

void vorbis_read_pcm_frames(ma_data_source *pDataSource, void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead)
{
        ma_libvorbis *vorbis = (ma_libvorbis *)pDataSource;
        AudioData *pAudioData = (AudioData *)vorbis->pReadSeekTellUserData;

        ma_uint64 framesRead = 0;

        while (framesRead < frameCount)
        {
                if (doQuit)
                        return;

                if (isImplSwitchReached())
                        return;


                if (pthread_mutex_trylock(&dataSourceMutex) != 0)
                {
                        return;
                }                        

                // Check if a file switch is required
                if (pAudioData->switchFiles)
                {
                        executeSwitch(pAudioData);
                        pthread_mutex_unlock(&dataSourceMutex);
                        break; // Exit the loop after the file switch
                }

                ma_libvorbis *decoder = getCurrentVorbisDecoder();                

                if ((getCurrentImplementationType() != VORBIS && !isSkipToNext()) || (decoder == NULL))
                {
                        pthread_mutex_unlock(&dataSourceMutex);
                        return;
                }


                // Check if seeking is requested
                if (isSeekRequested())
                {
                        // ma_uint64 cursor;
                        // ma_data_source_get_cursor_in_pcm_frames(decoder, &cursor);
                        //  FIXME Implement this properly
                        //  double percElapsed = getPercentageElapsed();
                        //  ma_uint64 approxTotalFrames = cursor / percElapsed;
                        //  ma_uint64 targetFrame = (approxTotalFrames * getSeekPercentage()) / 100;

                        // // // Set the read pointer for the decoder
                        // ma_result seekResult = ma_libvorbis_seek_to_pcm_frame(decoder, tagetFrame);
                        // if (seekResult != MA_SUCCESS)
                        // {

                        //         break;
                        // }
                        setSeekRequested(false);
                }

                // Read from the current decoder
                ma_uint64 framesToRead = 0;
                ma_result result;
                ma_uint64 remainingFrames = frameCount - framesRead;
                ma_libvorbis *firstDecoder = getFirstVorbisDecoder();

                if (firstDecoder == NULL)
                {
                        pthread_mutex_unlock(&dataSourceMutex);
                        return;
                }

                result = ma_data_source_read_pcm_frames(firstDecoder, (ma_int32 *)pFramesOut + framesRead * pAudioData->channels, remainingFrames, &framesToRead);

                if ((getPercentageElapsed() >= 1.0 || isSkipToNext() || result != MA_SUCCESS) &&
                    !isEOFReached())
                {
                        activateSwitch(pAudioData);
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
                        // Memory allocation failed
                        return;
                }
        }

        // No format conversion needed, just copy the audio samples
        memcpy(audioBuffer, pFramesOut, sizeof(ma_int32) * framesRead);
        setAudioBuffer(audioBuffer);

        if (pFramesRead != NULL)
        {
                *pFramesRead = framesRead;
        }
}

void vorbis_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount)
{
        AudioData *pDataSource = (AudioData *)pDevice->pUserData;
        ma_uint64 framesRead = 0;
        vorbis_read_pcm_frames(&pDataSource->base, pFramesOut, frameCount, &framesRead);
        (void)pFramesIn;
}