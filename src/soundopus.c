#include "soundopus.h"
/*

soundopus.c

 Functions related to miniaudio implementation for miniaudio opus decoding

*/

MA_API ma_result ma_libopus_read_pcm_frames_wrapper(void *pDecoder, void *pFramesOut, long unsigned int frameCount, long unsigned int *pFramesRead)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_libopus_read_pcm_frames((ma_libopus *)dec->pUserData, pFramesOut, frameCount, (ma_uint64 *)pFramesRead);
}

MA_API ma_result ma_libopus_seek_to_pcm_frame_wrapper(void *pDecoder, long long int frameIndex, ma_seek_origin origin)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_libopus_seek_to_pcm_frame((ma_libopus *)dec->pUserData, frameIndex);
}

MA_API ma_result ma_libopus_get_cursor_in_pcm_frames_wrapper(void *pDecoder, long long int *pCursor)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_libopus_get_cursor_in_pcm_frames((ma_libopus *)dec->pUserData, (ma_uint64 *)pCursor);
}

void opus_read_pcm_frames(ma_data_source *pDataSource, void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead)
{
        ma_libopus *opus = (ma_libopus *)pDataSource;
        PCMFileDataSource *pPCMDataSource = (PCMFileDataSource *)opus->pReadSeekTellUserData;

        ma_uint64 framesRead = 0;

        while (framesRead < frameCount)
        {
                if (doQuit)
                        return;

                if (isImplSwitchReached())
                        return;

                // Check if a file switch is required
                if (pPCMDataSource->switchFiles)
                {
                        executeSwitch(pPCMDataSource);
                        break; // Exit the loop after the file switch
                }

                if (getCurrentImplementationType() != OPUS && !isSkipToNext())
                        return;

                ma_libopus *decoder = getCurrentOpusDecoder();

                if (pPCMDataSource->totalFrames == 0)
                        ma_data_source_get_length_in_pcm_frames(decoder, &pPCMDataSource->totalFrames);

                // Check if seeking is requested
                if (isSeekRequested())
                {
                        ma_uint64 totalFrames = 0;
                        ma_libopus_get_length_in_pcm_frames(decoder, &totalFrames);
                        ma_uint64 seekPercent = getSeekPercentage();
                        if (seekPercent >= 100.0)
                                seekPercent = 100.0;
                        ma_uint64 targetFrame = (totalFrames * seekPercent) / 100;

                        // Set the read pointer for the decoder
                        ma_result seekResult = ma_libopus_seek_to_pcm_frame(decoder, targetFrame);
                        if (seekResult != MA_SUCCESS)
                        {
                                // Handle seek error
                                setSeekRequested(false);
                                return;
                        }

                        setSeekRequested(false); // Reset seek flag
                }

                // Read from the current decoder
                ma_uint64 framesToRead = 0;
                ma_result result;
                ma_uint64 remainingFrames = frameCount - framesRead;
                ma_decoder *firstDecoder = getFirstOpusDecoder();
                result = ma_data_source_read_pcm_frames(firstDecoder, (ma_int32 *)pFramesOut + framesRead * pPCMDataSource->channels, remainingFrames, &framesToRead);

                ma_uint64 cursor;

                ma_data_source_get_cursor_in_pcm_frames(decoder, &cursor);

                if (((cursor != 0 && cursor >= pPCMDataSource->totalFrames) || framesToRead == 0 || isSkipToNext() || result != MA_SUCCESS) && !isEOFReached())
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

void opus_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount)
{
        PCMFileDataSource *pDataSource = (PCMFileDataSource *)pDevice->pUserData;
        ma_uint64 framesRead = 0;
        opus_read_pcm_frames(&pDataSource->base, pFramesOut, frameCount, &framesRead);
        (void)pFramesIn;
}