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

static ma_result builtin_file_data_source_read(ma_data_source *p_data_source,
                                               void *p_frames_out,
                                               ma_uint64 frame_count,
                                               ma_uint64 *p_frames_read)
{
        // Dummy implementation
        (void)p_data_source;
        (void)p_frames_out;
        (void)frame_count;
        (void)p_frames_read;
        return MA_SUCCESS;
}

static ma_result builtin_file_data_source_seek(ma_data_source *p_data_source,
                                               ma_uint64 frame_index)
{
        if (p_data_source == NULL) {
                return MA_INVALID_ARGS;
        }

        AudioData *audio_data = (AudioData *)p_data_source;

        if (get_current_builtin_decoder() == NULL) {
                return MA_INVALID_ARGS;
        }

        ma_result result = ma_decoder_seek_to_pcm_frame(
            get_current_builtin_decoder(), frame_index);

        if (result == MA_SUCCESS) {
                audio_data->currentPCMFrame = frame_index;
                return MA_SUCCESS;
        } else {
                return result;
        }
}

static ma_result builtin_file_data_source_get_data_format(
    ma_data_source *p_data_source, ma_format *p_format, ma_uint32 *p_channels,
    ma_uint32 *p_sample_rate, ma_channel *p_channel_map, size_t channel_map_cap)
{
        (void)p_channel_map;
        (void)channel_map_cap;

        if (p_data_source == NULL) {
                return MA_INVALID_ARGS;
        }

        AudioData *audio_data = (AudioData *)p_data_source;

        if (p_format == NULL || p_channels == NULL || p_sample_rate == NULL) {
                return MA_INVALID_ARGS;
        }

        *p_format = audio_data->format;
        *p_channels = audio_data->channels;
        *p_sample_rate = audio_data->sample_rate;

        return MA_SUCCESS;
}

static ma_result
builtin_file_data_source_get_cursor(ma_data_source *p_data_source,
                                    ma_uint64 *p_cursor)
{
        if (p_data_source == NULL) {
                return MA_INVALID_ARGS;
        }

        AudioData *audio_data = (AudioData *)p_data_source;
        *p_cursor = audio_data->currentPCMFrame;

        return MA_SUCCESS;
}

static ma_result
builtin_file_data_source_get_length(ma_data_source *p_data_source,
                                    ma_uint64 *p_length)
{
        (void)p_data_source;
        ma_uint64 totalFrames = 0;

        if (get_current_builtin_decoder() == NULL) {
                return MA_INVALID_ARGS;
        }

        ma_result result = ma_decoder_get_length_in_pcm_frames(
            get_current_builtin_decoder(), &totalFrames);

        if (result != MA_SUCCESS) {
                return result;
        }

        *p_length = totalFrames;

        return MA_SUCCESS;
}

static ma_result
builtin_file_data_source_set_looping(ma_data_source *p_data_source,
                                     ma_bool32 is_looping)
{
        // Dummy implementation
        (void)p_data_source;
        (void)is_looping;

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

double db_to_linear(double db)
{
        return pow(10.0, db / 20.0);
}

bool is_valid_gain(double gain)
{
        return gain > -50.0 && gain < 50.0 && !isnan(gain) && isfinite(gain);
}

static bool compute_replay_gain(AudioData *audio_data, double *out_gain_db)
{
        if (audio_data->pUserData == NULL)
                return false;

        UserData *ud = audio_data->pUserData;

        bool result = false;
        double gain_db = 0.0;

        if ((!ud->songdataADeleted && ud->current_song_data == ud->songdataA) ||
            (!ud->songdataBDeleted && ud->current_song_data == ud->songdataB)) {
                SongData *song = ud->current_song_data;
                if (song != NULL && song->magic == SONG_MAGIC && song->metadata != NULL) {
                        double track_gain = song->metadata->replaygainTrack;
                        double album_gain = song->metadata->replaygainAlbum;

                        bool useTrackFirst = (ud->replayGainCheckFirst == 0);

                        if (useTrackFirst && is_valid_gain(track_gain)) {
                                gain_db = track_gain;
                                result = true;
                        } else if (is_valid_gain(album_gain)) {
                                gain_db = album_gain;
                                result = true;
                        } else if (!useTrackFirst && is_valid_gain(track_gain)) {
                                gain_db = track_gain;
                                result = true;
                        }
                }
        }

        if (result) {
                *out_gain_db = gain_db;
        }

        return result;
}

static void apply_gain_to_interleaved_frames(void *raw_frames, ma_format format,
                                             ma_uint64 frames_to_read, int channels,
                                             double gain)
{
        if (gain == 1.0) {
                return; // No gain to apply
        }

        // Prevent multiplication overflow:
        if (channels <= 0)
                return;

        if (frames_to_read > (UINT64_MAX / channels)) {
                // Overflow would happen
                return;
        }

        switch (format) {
        case ma_format_f32: {
                float *frames = (float *)raw_frames;
                for (ma_uint64 i = 0; i < frames_to_read; ++i) {
                        for (int ch = 0; ch < channels; ++ch) {
                                ma_uint64 frame_index = i * channels + ch;
                                float originalSample = frames[frame_index];
                                double sample = (double)originalSample;

                                sample *= gain;
                                frames[frame_index] = (float)sample;
                        }
                }
                break;
        }

        case ma_format_s16: {
                ma_int16 *frames = (ma_int16 *)raw_frames;
                for (ma_uint64 i = 0; i < frames_to_read; ++i) {
                        for (int ch = 0; ch < channels; ++ch) {
                                ma_uint64 frame_index = i * channels + ch;
                                ma_int16 originalSample = frames[frame_index];
                                double sample = (double)originalSample;

                                sample *= gain;
                                if (sample > 32767.0)
                                        sample = 32767.0;
                                else if (sample < -32768.0)
                                        sample = -32768.0;

                                frames[frame_index] = (ma_int16)sample;
                        }
                }
                break;
        }

        case ma_format_s32: {
                ma_int32 *frames = (ma_int32 *)raw_frames;
                for (ma_uint64 i = 0; i < frames_to_read; ++i) {
                        for (int ch = 0; ch < channels; ++ch) {
                                ma_uint64 frame_index = i * channels + ch;
                                ma_int32 originalSample = frames[frame_index];
                                double sample = (double)originalSample;

                                sample *= gain;
                                if (sample > 2147483647.0)
                                        sample = 2147483647.0;
                                else if (sample < -2147483648.0)
                                        sample = -2147483648.0;

                                frames[frame_index] = (ma_int32)sample;
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

ma_uint64 lastCursor = 0;

static bool should_switch(AudioData *audio_data, ma_uint64 frames_to_read,
                          ma_result result, ma_uint64 cursor)
{
        if (cursor != 0ULL && lastCursor == cursor)
        {
                lastCursor = 0;
                return true;
        }

        return (((audio_data->totalFrames != 0 &&
                  cursor >= audio_data->totalFrames) ||
                 frames_to_read == 0 || is_skip_to_next() || result != MA_SUCCESS) &&
                !pb_is_EOF_reached());
}

void builtin_read_pcm_frames(ma_data_source *p_data_source, void *p_frames_out,
                             ma_uint64 frame_count, ma_uint64 *p_frames_read)
{
        AudioData *audio_data = (AudioData *)p_data_source;
        ma_uint64 frames_read = 0;

        AppState *state = get_app_state();

        // Step 1: Compute gain
        while (frames_read < frame_count) {
                ma_uint64 remaining_frames = frame_count - frames_read;

                if (pthread_mutex_trylock(&(state->data_source_mutex)) != 0)
                        return;

                // Step 2: Handle file switching or state invalidation
                if (audio_data == NULL || audio_data->pUserData == NULL || pb_is_impl_switch_reached()) {
                        pthread_mutex_unlock(&(state->data_source_mutex));
                        return;
                }

                if (audio_data->switchFiles) {
                        execute_switch(audio_data);
                        pthread_mutex_unlock(&(state->data_source_mutex));
                        break;
                }

                ma_decoder *decoder = get_current_builtin_decoder();
                if ((get_current_implementation_type() != BUILTIN &&
                     !is_skip_to_next()) ||
                    decoder == NULL) {
                        pthread_mutex_unlock(&(state->data_source_mutex));
                        return;
                }

                // Step 3: Get total frames if needed
                if (audio_data->totalFrames == 0) {
                        ma_data_source_get_length_in_pcm_frames(
                            decoder, &(audio_data->totalFrames));
                }

                // Step 4: Seek if requested
                if (!perform_seek_if_requested(decoder, audio_data)) {
                        pthread_mutex_unlock(&(state->data_source_mutex));
                        return;
                }

                // Step 5: Read frames
                ma_uint64 frames_to_read = 0;
                ma_decoder *first_decoder = get_first_decoder();
                ma_uint64 cursor = 0;
                if (first_decoder == NULL || pb_is_EOF_reached()) {
                        pthread_mutex_unlock(&(state->data_source_mutex));
                        return;
                }

                double gain_db = 0.0;
                bool gainAvailable = compute_replay_gain(audio_data, &gain_db);
                double gain_factor = gainAvailable ? db_to_linear(gain_db) : 1.0;

                ma_result result = call_read_PCM_frames(
                    first_decoder, audio_data->format, p_frames_out, frames_read,
                    audio_data->channels, remaining_frames, &frames_to_read);

                ma_data_source_get_cursor_in_pcm_frames(decoder, &cursor);

                void *frame_start =
                    (void *)((float *)p_frames_out +
                             frames_read *
                                 audio_data->channels); // Cast matches format

                // Step 6: Apply gain
                if (gain_factor != 1.0) {
                        apply_gain_to_interleaved_frames(
                            frame_start, audio_data->format, frames_to_read,
                            audio_data->channels, gain_factor);
                }

                // Step 7: Check for switch
                if (should_switch(audio_data, frames_to_read, result, cursor)) {
                        activate_switch(audio_data);
                        pthread_mutex_unlock(&(state->data_source_mutex));
                        lastCursor = 0;
                        continue;
                }

                lastCursor = cursor;

                // Step 8: Update state
                frames_read += frames_to_read;
                set_buffer_size(frames_to_read);

                pthread_mutex_unlock(&(state->data_source_mutex));
        }

        // Step 9: Finalize
        set_audio_buffer(p_frames_out, frames_read, audio_data->sample_rate,
                         audio_data->channels, audio_data->format);

        if (p_frames_read != NULL)
                *p_frames_read = frames_read;
}

void builtin_on_audio_frames(ma_device *p_device, void *p_frames_out,
                             const void *p_frames_in, ma_uint32 frame_count)
{
        AudioData *p_data_source = (AudioData *)p_device->pUserData;
        ma_uint64 frames_read = 0;
        builtin_read_pcm_frames(&(p_data_source->base), p_frames_out, frame_count,
                                &frames_read);
        (void)p_frames_in;
}
