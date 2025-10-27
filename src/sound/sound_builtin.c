/**
 * @file sound_builtin.[c]
 * @brief Built-in audio backend implementation.
 *
 * Functions related to miniaudio implementation for miniaudio built-in decoders
 * (flac, wav and mp3).
 */

#include "sound_builtin.h"

#include "common/appstate.h"

#include "sound/audiobuffer.h"
#include "sound/decoders.h"
#include "sound/playback.h"
#include "sound/sound.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

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

        AudioData *audio_data = (AudioData *)pDataSource;

        if (get_current_builtin_decoder() == NULL)
        {
                return MA_INVALID_ARGS;
        }

        ma_result result = ma_decoder_seek_to_pcm_frame(
            get_current_builtin_decoder(), frameIndex);

        if (result == MA_SUCCESS)
        {
                audio_data->currentPCMFrame = frameIndex;
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

        AudioData *audio_data = (AudioData *)pDataSource;

        if (pFormat == NULL || pChannels == NULL || pSampleRate == NULL)
        {
                return MA_INVALID_ARGS;
        }

        *pFormat = audio_data->format;
        *pChannels = audio_data->channels;
        *pSampleRate = audio_data->sample_rate;

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

        AudioData *audio_data = (AudioData *)pDataSource;
        *pCursor = audio_data->currentPCMFrame;

        return MA_SUCCESS;
}

static ma_result
builtin_file_data_source_get_length(ma_data_source *pDataSource,
                                    ma_uint64 *pLength)
{
        (void)pDataSource;
        ma_uint64 totalFrames = 0;

        if (get_current_builtin_decoder() == NULL)
        {
                return MA_INVALID_ARGS;
        }

        ma_result result = ma_decoder_get_length_in_pcm_frames(
            get_current_builtin_decoder(), &totalFrames);

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

double db_to_linear(double db) { return pow(10.0, db / 20.0); }

bool is_valid_gain(double gain)
{
        return gain > -50.0 && gain < 50.0 && !isnan(gain) && isfinite(gain);
}

static bool compute_replay_gain(AudioData *audio_data, double *outGainDb)
{
        if (audio_data->pUserData == NULL)
                return false;

        UserData *ud = audio_data->pUserData;

        bool result = false;
        double gainDb = 0.0;

        if ((!ud->songdataADeleted && ud->currentSongData == ud->songdataA) ||
            (!ud->songdataBDeleted && ud->currentSongData == ud->songdataB))
        {
                SongData *song = ud->currentSongData;
                if (song != NULL && song->magic == SONG_MAGIC && song->metadata != NULL)
                {
                        double trackGain = song->metadata->replaygainTrack;
                        double albumGain = song->metadata->replaygainAlbum;

                        bool useTrackFirst = (ud->replayGainCheckFirst == 0);

                        if (useTrackFirst && is_valid_gain(trackGain))
                        {
                                gainDb = trackGain;
                                result = true;
                        }
                        else if (is_valid_gain(albumGain))
                        {
                                gainDb = albumGain;
                                result = true;
                        }
                        else if (!useTrackFirst && is_valid_gain(trackGain))
                        {
                                gainDb = trackGain;
                                result = true;
                        }
                }
        }

        if (result)
        {
                *outGainDb = gainDb;
        }

        return result;
}

static void apply_gain_to_interleaved_frames(void *rawFrames, ma_format format,
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

static bool perform_seek_if_requested(ma_decoder *decoder, AudioData *audio_data)
{
        if (!is_seek_requested())
                return true;

        ma_uint64 totalFrames = audio_data->totalFrames;
        double seek_percent = get_seek_percentage();
        if (seek_percent > 100.0)
                seek_percent = 100.0;

        ma_uint64 targetFrame =
            (ma_uint64)((totalFrames - 1) * seek_percent / 100.0);
        if (targetFrame >= totalFrames)
                targetFrame = totalFrames - 1;

        ma_result result = ma_decoder_seek_to_pcm_frame(decoder, targetFrame);
        set_seek_requested(false);

        return result == MA_SUCCESS;
}

static bool should_switch(AudioData *audio_data, ma_uint64 framesToRead,
                         ma_result result, ma_uint64 cursor)
{
        return (((audio_data->totalFrames != 0 &&
                  cursor >= audio_data->totalFrames) ||
                 framesToRead == 0 || is_skip_to_next() || result != MA_SUCCESS) &&
                !is_EOF_reached());
}

void builtin_read_pcm_frames(ma_data_source *pDataSource, void *pFramesOut,
                             ma_uint64 frameCount, ma_uint64 *pFramesRead)
{
        AudioData *audio_data = (AudioData *)pDataSource;
        ma_uint64 framesRead = 0;

        AppState *state = get_app_state();

        // Step 1: Compute gain
        while (framesRead < frameCount)
        {
                ma_uint64 remainingFrames = frameCount - framesRead;

                if (pthread_mutex_trylock(&(state->data_source_mutex)) != 0)
                        return;

                // Step 2: Handle file switching or state invalidation
                if (audio_data == NULL || audio_data->pUserData == NULL || is_impl_switch_reached())
                {
                        pthread_mutex_unlock(&(state->data_source_mutex));
                        return;
                }

                if (audio_data->switchFiles)
                {
                        execute_switch(audio_data);
                        pthread_mutex_unlock(&(state->data_source_mutex));
                        break;
                }

                ma_decoder *decoder = get_current_builtin_decoder();
                if ((get_current_implementation_type() != BUILTIN &&
                     !is_skip_to_next()) ||
                    decoder == NULL)
                {
                        pthread_mutex_unlock(&(state->data_source_mutex));
                        return;
                }

                // Step 3: Get total frames if needed
                if (audio_data->totalFrames == 0)
                {
                        ma_data_source_get_length_in_pcm_frames(
                            decoder, &(audio_data->totalFrames));
                }

                // Step 4: Seek if requested
                if (!perform_seek_if_requested(decoder, audio_data))
                {
                        pthread_mutex_unlock(&(state->data_source_mutex));
                        return;
                }

                // Step 5: Read frames
                ma_uint64 framesToRead = 0;
                ma_decoder *first_decoder = get_first_decoder();
                ma_uint64 cursor = 0;
                if (first_decoder == NULL || is_EOF_reached())
                {
                        pthread_mutex_unlock(&(state->data_source_mutex));
                        return;
                }

                double gainDb = 0.0;
                bool gainAvailable = compute_replay_gain(audio_data, &gainDb);
                double gainFactor = gainAvailable ? db_to_linear(gainDb) : 1.0;

                ma_result result = call_read_PCM_frames(
                    first_decoder, audio_data->format, pFramesOut, framesRead,
                    audio_data->channels, remainingFrames, &framesToRead);

                ma_data_source_get_cursor_in_pcm_frames(decoder, &cursor);

                void *frameStart =
                    (void *)((float *)pFramesOut +
                             framesRead *
                                 audio_data->channels); // Cast matches format

                // Step 6: Apply gain
                if (gainFactor != 1.0)
                {
                        apply_gain_to_interleaved_frames(
                            frameStart, audio_data->format, framesToRead,
                            audio_data->channels, gainFactor);
                }

                // Step 7: Check for switch
                if (should_switch(audio_data, framesToRead, result, cursor))
                {
                        activate_switch(audio_data);
                        pthread_mutex_unlock(&(state->data_source_mutex));
                        continue;
                }

                // Step 8: Update state
                framesRead += framesToRead;
                set_buffer_size(framesToRead);

                pthread_mutex_unlock(&(state->data_source_mutex));
        }

        // Step 9: Finalize
        set_audio_buffer(pFramesOut, framesRead, audio_data->sample_rate,
                       audio_data->channels, audio_data->format);

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
