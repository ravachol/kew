#include "soundbuiltin.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/*

soundbuiltin.c

 Functions related to miniaudio implementation for miniaudio built-in decoders
(flac, wav and mp3)

*/

static ma_result builtin_file_data_source_read(ma_data_source *pDataSource,
                                               void *pFramesOut,
                                               ma_uint64 frameCount,
                                               ma_uint64 *pFramesRead)
{
        // Dummy implementation
        (void)pDataSource;
        (void)pFramesOut;
        (void)frameCount;
        (void)pFramesRead;
        return MA_SUCCESS;
}

static ma_result builtin_file_data_source_seek(ma_data_source *pDataSource,
                                               ma_uint64 frameIndex)
{
        if (pDataSource == NULL)
        {
                return MA_INVALID_ARGS;
        }

        AudioData *audioData = (AudioData *)pDataSource;

        if (getCurrentBuiltinDecoder() == NULL)
        {
                return MA_INVALID_ARGS;
        }

        ma_result result = ma_decoder_seek_to_pcm_frame(
            getCurrentBuiltinDecoder(), frameIndex);

        if (result == MA_SUCCESS)
        {
                audioData->currentPCMFrame = frameIndex;
                return MA_SUCCESS;
        }
        else
        {
                return result;
        }
}

static ma_result builtin_file_data_source_get_data_format(
    ma_data_source *pDataSource, ma_format *pFormat, ma_uint32 *pChannels,
    ma_uint32 *pSampleRate, ma_channel *pChannelMap, size_t channelMapCap)
{
        (void)pChannelMap;
        (void)channelMapCap;

        if (pDataSource == NULL)
        {
                return MA_INVALID_ARGS;
        }

        AudioData *audioData = (AudioData *)pDataSource;

        if (pFormat == NULL || pChannels == NULL || pSampleRate == NULL)
        {
                return MA_INVALID_ARGS;
        }

        *pFormat = audioData->format;
        *pChannels = audioData->channels;
        *pSampleRate = audioData->sampleRate;

        return MA_SUCCESS;
}

static ma_result
builtin_file_data_source_get_cursor(ma_data_source *pDataSource,
                                    ma_uint64 *pCursor)
{
        if (pDataSource == NULL)
        {
                return MA_INVALID_ARGS;
        }

        AudioData *audioData = (AudioData *)pDataSource;
        *pCursor = audioData->currentPCMFrame;

        return MA_SUCCESS;
}

static ma_result
builtin_file_data_source_get_length(ma_data_source *pDataSource,
                                    ma_uint64 *pLength)
{
        (void)pDataSource;
        ma_uint64 totalFrames = 0;

        if (getCurrentBuiltinDecoder() == NULL)
        {
                return MA_INVALID_ARGS;
        }

        ma_result result = ma_decoder_get_length_in_pcm_frames(
            getCurrentBuiltinDecoder(), &totalFrames);

        if (result != MA_SUCCESS)
        {
                return result;
        }

        *pLength = totalFrames;

        return MA_SUCCESS;
}

static ma_result
builtin_file_data_source_set_looping(ma_data_source *pDataSource,
                                     ma_bool32 isLooping)
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
    0 // Flags
};

double dbToLinear(double db) { return pow(10.0, db / 20.0); }

bool isValidGain(double gain)
{
        return gain > -50.0 && gain < 50.0 && !isnan(gain) && isfinite(gain);
}

static bool computeReplayGain(AudioData *audioData, double *outGainDb)
{
    if (audioData == NULL || audioData->pUserData == NULL)
        return false;

    UserData *ud = audioData->pUserData;

    bool result = false;
    double gainDb = 0.0;

    if ((!ud->songdataADeleted && ud->currentSongData == ud->songdataA) ||
        (!ud->songdataBDeleted && ud->currentSongData == ud->songdataB))
    {
        SongData *song = ud->currentSongData;
        if (song != NULL && song->metadata != NULL)
        {
            double trackGain = song->metadata->replaygainTrack;
            double albumGain = song->metadata->replaygainAlbum;

            bool useTrackFirst = (ud->replayGainCheckFirst == 0);

            if (useTrackFirst && isValidGain(trackGain))
            {
                gainDb = trackGain;
                result = true;
            }
            else if (isValidGain(albumGain))
            {
                gainDb = albumGain;
                result = true;
            }
            else if (!useTrackFirst && isValidGain(trackGain))
            {
                gainDb = trackGain;
                result = true;
            }
        }
    }

    if (result) {
        *outGainDb = gainDb;
    }

    return result;
}

static void applyGainToInterleavedFrames(void *rawFrames, ma_format format,
                                         ma_uint64 framesToRead, int channels,
                                         double gain)
{
        if (gain == 1.0)
        {
                return; // No gain to apply
        }

        // Prevent multiplication overflow:
        if (channels <= 0)
                return;

        if (framesToRead > (UINT64_MAX / channels))
        {
                // Overflow would happen
                return;
        }

        switch (format)
        {
        case ma_format_f32:
        {
                float *frames = (float *)rawFrames;
                for (ma_uint64 i = 0; i < framesToRead; ++i)
                {
                        for (int ch = 0; ch < channels; ++ch)
                        {
                                ma_uint64 frameIndex = i * channels + ch;
                                float originalSample = frames[frameIndex];
                                double sample = (double)originalSample;

                                sample *= gain;
                                frames[frameIndex] = (float)sample;
                        }
                }
                break;
        }

        case ma_format_s16:
        {
                ma_int16 *frames = (ma_int16 *)rawFrames;
                for (ma_uint64 i = 0; i < framesToRead; ++i)
                {
                        for (int ch = 0; ch < channels; ++ch)
                        {
                                ma_uint64 frameIndex = i * channels + ch;
                                ma_int16 originalSample = frames[frameIndex];
                                double sample = (double)originalSample;

                                sample *= gain;
                                if (sample > 32767.0)
                                        sample = 32767.0;
                                else if (sample < -32768.0)
                                        sample = -32768.0;

                                frames[frameIndex] = (ma_int16)sample;
                        }
                }
                break;
        }

        case ma_format_s32:
        {
                ma_int32 *frames = (ma_int32 *)rawFrames;
                for (ma_uint64 i = 0; i < framesToRead; ++i)
                {
                        for (int ch = 0; ch < channels; ++ch)
                        {
                                ma_uint64 frameIndex = i * channels + ch;
                                ma_int32 originalSample = frames[frameIndex];
                                double sample = (double)originalSample;

                                sample *= gain;
                                if (sample > 2147483647.0)
                                        sample = 2147483647.0;
                                else if (sample < -2147483648.0)
                                        sample = -2147483648.0;

                                frames[frameIndex] = (ma_int32)sample;
                        }
                }
                break;
        }

        default:
                // Unsupported format
                break;
        }
}

static bool performSeekIfRequested(ma_decoder *decoder, AudioData *audioData)
{
        if (!isSeekRequested())
                return true;

        ma_uint64 totalFrames = audioData->totalFrames;
        double seekPercent = getSeekPercentage();
        if (seekPercent > 100.0)
                seekPercent = 100.0;

        ma_uint64 targetFrame =
            (ma_uint64)((totalFrames - 1) * seekPercent / 100.0);
        if (targetFrame >= totalFrames)
                targetFrame = totalFrames - 1;

        ma_result result = ma_decoder_seek_to_pcm_frame(decoder, targetFrame);
        setSeekRequested(false);

        return result == MA_SUCCESS;
}

static bool shouldSwitch(AudioData *audioData, ma_uint64 framesToRead,
                         ma_result result, ma_uint64 cursor)
{
        return (((audioData->totalFrames != 0 &&
                  cursor >= audioData->totalFrames) ||
                 framesToRead == 0 || isSkipToNext() || result != MA_SUCCESS) &&
                !isEOFReached());
}

void builtin_read_pcm_frames(ma_data_source *pDataSource, void *pFramesOut,
                             ma_uint64 frameCount, ma_uint64 *pFramesRead)
{
        AudioData *audioData = (AudioData *)pDataSource;
        ma_uint64 framesRead = 0;

        // Step 1: Compute gain
        double gainDb = 0.0;
        bool gainAvailable = computeReplayGain(audioData, &gainDb);
        double gainFactor = gainAvailable ? dbToLinear(gainDb) : 1.0;

        while (framesRead < frameCount)
        {
                ma_uint64 remainingFrames = frameCount - framesRead;

                if (pthread_mutex_trylock(&dataSourceMutex) != 0)
                        return;

                // Step 2: Handle file switching or state invalidation
                if (audioData == NULL || isImplSwitchReached())
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
                if ((getCurrentImplementationType() != BUILTIN &&
                     !isSkipToNext()) ||
                    decoder == NULL)
                {
                        pthread_mutex_unlock(&dataSourceMutex);
                        return;
                }

                // Step 3: Get total frames if needed
                if (audioData->totalFrames == 0)
                {
                        ma_data_source_get_length_in_pcm_frames(
                            decoder, &(audioData->totalFrames));
                }

                // Step 4: Seek if requested
                if (!performSeekIfRequested(decoder, audioData))
                {
                        pthread_mutex_unlock(&dataSourceMutex);
                        return;
                }

                // Step 5: Read frames
                ma_uint64 framesToRead = 0;
                ma_decoder *firstDecoder = getFirstDecoder();
                ma_uint64 cursor = 0;
                if (firstDecoder == NULL || isEOFReached())
                {
                        pthread_mutex_unlock(&dataSourceMutex);
                        return;
                }

                ma_result result = callReadPCMFrames(
                    firstDecoder, audioData->format, pFramesOut, framesRead,
                    audioData->channels, remainingFrames, &framesToRead);

                ma_data_source_get_cursor_in_pcm_frames(decoder, &cursor);

                void *frameStart =
                    (void *)((float *)pFramesOut +
                             framesRead *
                                 audioData->channels); // Cast matches format

                // Step 6: Apply gain
                if (gainFactor != 1.0)
                {
                        applyGainToInterleavedFrames(
                            frameStart, audioData->format, framesToRead,
                            audioData->channels, gainFactor);
                }

                // Step 7: Check for switch
                if (shouldSwitch(audioData, framesToRead, result, cursor))
                {
                        activateSwitch(audioData);
                        pthread_mutex_unlock(&dataSourceMutex);
                        continue;
                }

                // Step 8: Update state
                framesRead += framesToRead;
                setBufferSize(framesToRead);

                pthread_mutex_unlock(&dataSourceMutex);
        }

        // Step 9: Finalize
        setAudioBuffer(pFramesOut, framesRead, audioData->sampleRate,
                       audioData->channels, audioData->format);

        if (pFramesRead != NULL)
                *pFramesRead = framesRead;
}

void builtin_on_audio_frames(ma_device *pDevice, void *pFramesOut,
                             const void *pFramesIn, ma_uint32 frameCount)
{
        AudioData *pDataSource = (AudioData *)pDevice->pUserData;
        ma_uint64 framesRead = 0;
        builtin_read_pcm_frames(&(pDataSource->base), pFramesOut, frameCount,
                                &framesRead);
        (void)pFramesIn;
}
