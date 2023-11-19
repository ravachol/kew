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
        PCMFileDataSource *pPCMDataSource = (PCMFileDataSource *)vorbis->pReadSeekTellUserData;

        ma_uint64 framesRead = 0;

        while (framesRead < frameCount)
        {
                if (doQuit)
                        return;

                // Check if a file switch is required
                if (pPCMDataSource->switchFiles)
                {
                        executeSwitch(pPCMDataSource);
                        break; // Exit the loop after the file switch
                }

                 if (getCurrentImplementationType() != VORBIS && !isSkipToNext())
                        return;

                ma_libvorbis *decoder = getCurrentVorbisDecoder();

                if (decoder == NULL)
                        return;

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
                ma_uint64 length;
                ma_uint64 remainingFrames = frameCount - framesRead;
                result = ma_data_source_read_pcm_frames(getFirstVorbisDecoder(), (ma_int32 *)pFramesOut + framesRead * pPCMDataSource->channels, remainingFrames, &framesToRead);

                if ((getPercentageElapsed() >= 1.0 || isSkipToNext() || result != MA_SUCCESS) &&
                    !isEOFReached())
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
        PCMFileDataSource *pDataSource = (PCMFileDataSource *)pDevice->pUserData;
        ma_uint64 framesRead = 0;
        vorbis_read_pcm_frames(&pDataSource->base, pFramesOut, frameCount, &framesRead);
        (void)pFramesIn;
}