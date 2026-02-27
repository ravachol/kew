#include "ops/playback_ops.h"
#define MA_EXPERIMENTAL__DATA_LOOPING_AND_CHAINING
#define MA_NO_ENGINE
#define MINIAUDIO_IMPLEMENTATION

/**
 * @file sound.c
 * @brief High-level audio playback interface.
 *
 * Provides a unified API for creating an audio device
 * and switching decoders.
 */

#include "sound.h"

#include "common/appstate.h"
#include "common/common.h"

#include "utils/file.h"

#include "playback.h"
#ifdef USE_FAAD
#include "m4a.h"
#endif
#include "audio_file_info.h"
#include "audiobuffer.h"
#include "audiotypes.h"
#include "decoders.h"
#include "volume.h"

#include "utils/utils.h"

#include <miniaudio.h>

#include <fcntl.h>
#include <miniaudio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MIN_WRITE_THRESHOLD (CHUNK / 4)

static ma_context context;
static bool context_initialized = false;
long long lastCursor = 0;

int create_audio_device(
    UserData *user_data,
    AudioData *audio_data,
    ma_device *device,
    ma_context *context,
    decoder_getter_func get_decoder,
    ma_data_callback_proc callback);

bool valid_file_path(char *file_path)
{
        if (file_path == NULL || file_path[0] == '\0' || file_path[0] == '\r')
                return false;

        if (exists_file(file_path) < 0)
                return false;

        return true;
}

long long get_file_size(const char *filename)
{
        struct stat st;
        if (stat(filename, &st) == 0) {
                return (long long)st.st_size;
        } else {
                return -1;
        }
}

int calc_avg_bit_rate(double duration, const char *file_path)
{
        long long file_size = get_file_size(file_path); // in bytes
        int avg_bit_rate = 0;

        if (duration > 0.0)
                avg_bit_rate = (int)((file_size * 8.0) / duration /
                                     1000.0); // use 1000 for kbps

        return avg_bit_rate;
}

static bool should_switch(ma_uint64 frames_to_read,
                          ma_result result, ma_uint64 cursor)
{
        bool at_end = (((cursor != 0ULL && cursor == (ma_uint64)lastCursor) ||
                        frames_to_read == 0 || is_skip_to_next() ||
                        result != MA_SUCCESS) &&
                       !pb_is_EOF_reached());

        return at_end;
}

static bool execute_seek(ma_decoder *decoder, ma_uint64 targetFrame, const CodecOps *ops)
{
        stop_playback();

        ma_result result = ops->seek_to_pcm_frame(decoder, targetFrame, 0);

        if (result == MA_SUCCESS) {
                ma_pcm_rb_reset(&audio_data.pcm_rb);
                atomic_store(&audio_data.buffer_ready, 0);
                atomic_store(&audio_data.decode_finished, false);
        }

        resume_playback();

        return result;
}

static bool perform_seek_if_requested(ma_decoder *decoder, AudioData *audio_data)
{
        if (!is_seek_requested())
                return true;

        set_seek_requested(false);

        ma_uint64 totalFrames = audio_data->totalFrames;
        double seek_percent = get_seek_percentage();
        if (seek_percent > 100.0)
                seek_percent = 100.0;

        ma_uint64 targetFrame =
            (ma_uint64)((totalFrames - 1) * seek_percent / 100.0);
        if (targetFrame >= totalFrames)
                targetFrame = totalFrames - 1;

        const CodecOps *ops = get_codec_ops(audio_data->implType);

        return execute_seek(decoder, targetFrame, ops);
}

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

void *decode_loop(void *arg)
{
        AudioData *audio_data = (AudioData *)arg;

        float temp[audio_data->chunk_frames * audio_data->channels];

        AppState *state = get_app_state();
        if (!state)
                goto done;

        while (atomic_load(&audio_data->decode_thread_running)) {

                // Activate switch
                if (atomic_load(&audio_data->pending_switch)) {

                        atomic_store(&audio_data->pending_switch, false);

                        while (pthread_mutex_trylock(&state->data_source_mutex) != 0) {
                                if (!atomic_load(&audio_data->decode_thread_running))
                                        goto done;
                                c_sleep(1);
                        }

                        atomic_store(&audio_data->decode_finished, false);

                        activate_switch(audio_data);

                        pthread_mutex_unlock(&state->data_source_mutex);
                        continue;
                }

                // Execute switch
                if (audio_data->switch_files) {

                        pthread_mutex_lock(&state->data_source_mutex);

                        atomic_store(&audio_data->buffer_ready, 0);
                        atomic_store(&audio_data->decode_finished, false);

                        execute_switch(audio_data);

                        pthread_mutex_unlock(&state->data_source_mutex);
                        continue;
                }

                // Pause
                if (pb_is_paused()) {
                        c_sleep(10);
                        continue;
                }

                // Wait for ring buffer to have enough space to fill
                pthread_mutex_lock(&audio_data->rb_mutex);
                while (ma_pcm_rb_available_write(&audio_data->pcm_rb) < audio_data->chunk_frames / 4 &&
                       atomic_load(&audio_data->decode_thread_running) &&
                       !audio_data->switch_files &&
                       !atomic_load(&audio_data->pending_switch)) {

                        struct timespec ts;
                        clock_gettime(CLOCK_REALTIME, &ts);
                        ts.tv_nsec += 10 * 1000000;
                        if (ts.tv_nsec >= 1000000000) {
                                ts.tv_sec += 1;
                                ts.tv_nsec -= 1000000000;
                        }

                        atomic_store(&audio_data->buffer_ready, 1);

                        pthread_cond_timedwait(&audio_data->rb_cond, &audio_data->rb_mutex, &ts);
                }

                // Get a writable chunk
                ma_uint32 writable = ma_pcm_rb_available_write(&audio_data->pcm_rb);
                pthread_mutex_unlock(&audio_data->rb_mutex);

                if (!atomic_load(&audio_data->decode_thread_running))
                        goto done;

                if (writable == 0)
                        continue;

                ma_uint32 frames_to_decode = writable;
                if (frames_to_decode > audio_data->chunk_frames)
                        frames_to_decode = audio_data->chunk_frames;

                // Lock decoder
                while (pthread_mutex_trylock(&state->data_source_mutex) != 0) {
                        if (!atomic_load(&audio_data->decode_thread_running))
                                goto done;
                        c_sleep(1);
                }

                if (audio_data->pUserData == NULL || pb_is_impl_switch_reached()) {
                        pthread_mutex_unlock(&state->data_source_mutex);
                        goto done;
                }

                const CodecOps *ops = get_codec_ops(audio_data->implType);
                void *decoder = get_current_decoder();

                if (!decoder || pb_is_EOF_reached()) {
                        pthread_mutex_unlock(&state->data_source_mutex);
                        c_sleep(1);
                        continue;
                }

                if (audio_data->totalFrames == 0)
                        ma_data_source_get_length_in_pcm_frames(decoder, &audio_data->totalFrames);

                if (!perform_seek_if_requested(decoder, audio_data)) {
                        pthread_mutex_unlock(&state->data_source_mutex);
                        continue;
                }

                // Decode
                ma_uint64 frames_to_read = 0;
                ma_result result = ma_data_source_read_pcm_frames(
                    decoder,
                    temp,
                    frames_to_decode,
                    &frames_to_read);

                long long cursor = 0;
                ops->get_cursor(decoder, &cursor);

                double gain_db = 0.0;
                if (audio_data->implType == BUILTIN &&
                    compute_replay_gain(audio_data, &gain_db)) {

                        apply_gain_to_interleaved_frames(
                            temp,
                            audio_data->format,
                            frames_to_read,
                            audio_data->channels,
                            db_to_linear(gain_db));
                }

                if (!atomic_load(&audio_data->pending_switch) &&
                    !atomic_load(&audio_data->decode_finished) &&
                    should_switch(frames_to_read, result, (ma_uint64)cursor)) {

                        if (cursor != lastCursor)
                                atomic_store(&audio_data->pending_switch, true);
                        else {
                                atomic_store(&audio_data->decode_finished, true);
                                audio_data->track_end_frame =
                                    atomic_load(&audio_data->track_frames_sent) + ma_pcm_rb_available_read(&audio_data->pcm_rb);
                        }
                        pthread_mutex_unlock(&state->data_source_mutex);
                        continue;
                }

                lastCursor = cursor;

                pthread_mutex_unlock(&state->data_source_mutex);

                // Write to ring buffer
                if (frames_to_read > 0) {

                        ma_uint32 framesRemaining = (ma_uint32)frames_to_read;
                        ma_uint32 framesWritten = 0;

                        while (framesRemaining > 0) {

                                if (!atomic_load(&audio_data->decode_thread_running))
                                        goto done;

                                ma_uint32 framesToWrite = framesRemaining;
                                void *pWriteBuffer = NULL;

                                ma_result wr = ma_pcm_rb_acquire_write(
                                    &audio_data->pcm_rb,
                                    &framesToWrite,
                                    &pWriteBuffer);

                                if (wr != MA_SUCCESS || framesToWrite == 0 || pWriteBuffer == NULL)
                                        break;

                                MA_COPY_MEMORY(
                                    pWriteBuffer,
                                    (float *)temp + framesWritten * audio_data->channels,
                                    framesToWrite * audio_data->channels * sizeof(float));

                                ma_pcm_rb_commit_write(&audio_data->pcm_rb, framesToWrite);

                                framesWritten += framesToWrite;
                                framesRemaining -= framesToWrite;

                                if (!atomic_load(&audio_data->buffer_ready))
                                        atomic_store(&audio_data->buffer_ready, 1);
                        }
                }
        }

done:
        audio_data->decode_thread_active = false;
        return NULL;
}

int underrun_count = 0;

void on_audio_frames(ma_device *device,
                     void *pOutput,
                     const void *input,
                     ma_uint32 frameCount)
{
        (void)device;
        (void)input;

        if (!atomic_load(&audio_data.buffer_ready)) {
                memset(pOutput, 0, frameCount * audio_data.channels * sizeof(float));
                return;
        }

        const ma_uint32 frameSize = audio_data.channels * sizeof(float);
        ma_uint32 framesRemaining = frameCount;
        ma_uint32 totalFramesRead = 0;

        while (framesRemaining > 0) {

                ma_uint32 framesToRead = framesRemaining;
                void *pReadBuffer = NULL;

                ma_result result =
                    ma_pcm_rb_acquire_read(&audio_data.pcm_rb,
                                           &framesToRead,
                                           &pReadBuffer);

                if (result != MA_SUCCESS || framesToRead == 0 || pReadBuffer == NULL) {

                        memset((uint8_t *)pOutput + totalFramesRead * frameSize,
                               0,
                               framesRemaining * frameSize);

                        totalFramesRead += framesRemaining;
                        break;
                }

                memcpy((uint8_t *)pOutput + totalFramesRead * frameSize,
                       pReadBuffer,
                       framesToRead * frameSize);

                ma_pcm_rb_commit_read(&audio_data.pcm_rb, framesToRead);

                totalFramesRead += framesToRead;
                framesRemaining -= framesToRead;

                atomic_fetch_add(&audio_data.track_frames_sent, framesToRead);

                if (atomic_load(&audio_data.decode_finished)) {
                        uint64_t played =
                            atomic_load(&audio_data.track_frames_sent);

                        uint64_t boundary =
                            atomic_load(&audio_data.track_end_frame);

                        if (played >= boundary) {
                                atomic_store(&audio_data.pending_switch, true);
                        }
                }

                /* Wake decode thread when ring buffer has meaningful space available. */
                ma_uint32 spaceAvailable = ma_pcm_rb_available_write(&audio_data.pcm_rb);

                if (spaceAvailable >= audio_data.chunk_frames / 4) {
                        pthread_mutex_lock(&audio_data.rb_mutex);
                        pthread_cond_signal(&audio_data.rb_cond);
                        pthread_mutex_unlock(&audio_data.rb_mutex);
                }
        }

        set_audio_buffer(pOutput,
                         totalFramesRead,
                         audio_data.sample_rate,
                         audio_data.channels,
                         audio_data.format);
}

int handle_codec(
    const char *file_path,
    CodecOps ops,
    AudioData *audio_data,
    AppState *state,
    ma_context *context)
{
        ma_uint32 sample_rate, channels, nSampleRate, nChannels;
        ma_format format, nFormat;
        ma_channel channel_map[MA_MAX_CHANNELS], nChannelMap[MA_MAX_CHANNELS];
        enum AudioImplementation current_implementation =
            get_current_implementation_type();

        int avg_bit_rate = 0;

#ifdef USE_FAAD
        k_m4adec_filetype file_type = 0;

        if (ops.implType == M4A) {
                get_m4a_file_info_full(file_path, &format, &channels, &sample_rate, channel_map, &avg_bit_rate, &file_type);
        } else {
                ops.get_file_info(file_path, &format, &channels, &sample_rate, channel_map);
        }
#else
        ops.get_file_info(file_path, &format, &channels, &sample_rate, channel_map);
#endif

        format = ma_format_f32; // Hardcoded, float is used throughout

        void *decoder = get_current_decoder();
        if (decoder != NULL && ops.get_decoder_format)
                ops.get_decoder_format(decoder, &nFormat, &nChannels, &nSampleRate,
                                       nChannelMap, MA_MAX_CHANNELS);

        // sameFormat computation
        bool sameFormat = false;
        if (ops.supportsGapless && decoder != NULL) {
                sameFormat = (ops.implType == get_current_decoder_implType() && format == nFormat && channels == nChannels &&
                              sample_rate == nSampleRate);
#ifdef USE_FAAD
                if (ops.implType == M4A && decoder != NULL) {
                        sameFormat = sameFormat &&
                                     (((ma_m4a *)decoder)->file_type == file_type) &&
                                     (((ma_m4a *)decoder)->file_type != k_rawAAC);
                }
#endif
        }

        // Avg bitrate assignment
        if (audio_data->pUserData->current_song_data) {
                if (ops.implType == M4A)
                        audio_data->pUserData->current_song_data->avg_bit_rate = audio_data->avg_bit_rate = avg_bit_rate;
                else
                        audio_data->pUserData->current_song_data->avg_bit_rate =
                            audio_data->avg_bit_rate =
                                calc_avg_bit_rate(audio_data->pUserData->current_song_data->duration, file_path);
        } else {
                audio_data->avg_bit_rate = 0;
        }

        audio_data->implType = ops.implType;

        if (pb_is_repeat_enabled() || !(sameFormat && current_implementation == ops.implType)) {
                set_impl_switch_reached();

                cleanup_playback_device();

                pthread_mutex_lock(&(state->data_source_mutex));

                set_current_implementation_type(ops.implType);

                reset_decoders();
                reset_audio_buffer();

                audio_data->sample_rate = sample_rate;

                int result;

                if (ops.implType == BUILTIN) {
                        audio_data->base.vtable = &builtin_file_data_source_vtable;
                }

                result = create_audio_device(
                    audio_data->pUserData,
                    audio_data,
                    get_device(),
                    context,
                    get_current_decoder,
                    on_audio_frames);

                if (result < 0) {
                        set_current_implementation_type(NONE);
                        set_impl_switch_not_reached();
                        set_EOF_reached();
                        pthread_mutex_unlock(&(state->data_source_mutex));
                        return -1;
                }

                // Reset buffer state for new song
                atomic_store(&audio_data->buffer_ready, 0);
                atomic_store(&audio_data->decode_thread_running, true);
                audio_data->decode_thread_active = true;
                pthread_create(&audio_data->decode_thread,
                               NULL,
                               decode_loop,
                               audio_data);

                pthread_mutex_unlock(&(state->data_source_mutex));
                set_impl_switch_not_reached();
        }

        return 0;
}

static void handle_builtin_avg_bit_rate(AudioData *audio_data, SongData *song_data, const char *file_path)
{
        if (path_ends_with(file_path, ".mp3") && song_data) {
                int avg_bit_rate = calc_avg_bit_rate(song_data->duration, file_path);
                if (avg_bit_rate > 320)
                        avg_bit_rate = 320;
                song_data->avg_bit_rate = audio_data->avg_bit_rate = avg_bit_rate;
        } else {
                audio_data->avg_bit_rate = 0;
        }
}

static int init_audio_data_from_codec_decoder(const CodecOps *ops, void *decoder, AudioData *audio_data)
{
        if (!ops || !decoder || !audio_data)
                return -1;

        ma_channel channel_map[MA_MAX_CHANNELS];

        switch (ops->implType) {
        case BUILTIN: {
                ma_decoder *d = (ma_decoder *)decoder;
                audio_data->channels = d->outputChannels;
                audio_data->sample_rate = d->outputSampleRate;
                ma_data_source_get_length_in_pcm_frames((ma_data_source *)d, &audio_data->totalFrames);
                break;
        }

        case OPUS: {
                ma_libopus *d = (ma_libopus *)decoder;
                ma_libopus_ds_get_data_format(d, &audio_data->format, &audio_data->channels,
                                              &audio_data->sample_rate, channel_map, MA_MAX_CHANNELS);
                ma_data_source_get_length_in_pcm_frames((ma_data_source *)d, &audio_data->totalFrames);

                ((ma_data_source_base *)d)->pCurrent = d;
                d->pReadSeekTellUserData = audio_data;
                break;
        }

        case VORBIS: {
                ma_libvorbis *d = (ma_libvorbis *)decoder;
                ma_libvorbis_ds_get_data_format(d, &audio_data->format, &audio_data->channels,
                                                &audio_data->sample_rate, channel_map, MA_MAX_CHANNELS);
                ma_data_source_get_length_in_pcm_frames((ma_data_source *)d, &audio_data->totalFrames);

                ((ma_data_source_base *)d)->pCurrent = d;
                d->pReadSeekTellUserData = audio_data;
                break;
        }

        case WEBM: {
                ma_webm *d = (ma_webm *)decoder;
                ma_webm_ds_get_data_format(d, &audio_data->format, &audio_data->channels,
                                           &audio_data->sample_rate, channel_map, MA_MAX_CHANNELS);
                ma_data_source_get_length_in_pcm_frames((ma_data_source *)d, &audio_data->totalFrames);

                ((ma_data_source_base *)d)->pCurrent = d;
                d->pReadSeekTellUserData = audio_data;
                break;
        }

#ifdef USE_FAAD
        case M4A: {
                ma_m4a *d = (ma_m4a *)decoder;
                m4a_decoder_ds_get_data_format(d, &audio_data->format, &audio_data->channels,
                                               &audio_data->sample_rate, channel_map, MA_MAX_CHANNELS);
                ma_data_source_get_length_in_pcm_frames((ma_data_source *)d, &audio_data->totalFrames);

                ((ma_data_source_base *)d)->pCurrent = d;
                d->pReadSeekTellUserData = audio_data;
                break;
        }
#endif

        default:
                return -1;
        }

        audio_data->format = ma_format_f32; // hard-coded. float is used throughout

        return 0;
}

int init_first_datasource(UserData *user_data)
{
        if (!user_data)
                return MA_ERROR;

        SongData *song_data = (audio_data.currentFileIndex == 0) ? user_data->songdataA : user_data->songdataB;
        if (!song_data)
                return MA_ERROR;

        const char *file_path = song_data->file_path;
        if (!file_path)
                return MA_ERROR;

        audio_data.pUserData = user_data;
        audio_data.currentPCMFrame = 0;
        audio_data.restart = false;

        const CodecOps *ops = find_codec_ops(file_path);
        if (!ops)
                return MA_ERROR;

        int result = prepare_next_decoder(file_path, song_data, ops);
        if (result < 0)
                return -1;

        void *decoder = get_first_decoder();
        if (!decoder)
                return -1;

        result = init_audio_data_from_codec_decoder(ops, decoder, &audio_data);
        if (result < 0)
                return -1;

        // BUILTIN MP3 special handling
        if (ops->implType == BUILTIN)
                handle_builtin_avg_bit_rate(&audio_data, song_data, file_path);

        return MA_SUCCESS;
}

int init_ring_buffer(AudioData *audio_data)
{
        // 3 seconds of buffer regardless of sample rate
        ma_uint32 rbFrames = audio_data->sample_rate * 3;

        atomic_store(&audio_data->buffer_ready, 0);
        atomic_store(&audio_data->decode_finished, false);
        atomic_store(&audio_data->pending_switch, false);

        safe_ringbuffer_reset(&audio_data->pcm_rb);

        audio_data->pcm_rb.rb.pBuffer = NULL;
        audio_data->pcm_rb.rb.ownsBuffer = false;

        ma_result rbResult = ma_pcm_rb_init(audio_data->format,
                                            audio_data->channels,
                                            rbFrames,
                                            NULL,
                                            NULL,
                                            &audio_data->pcm_rb);
        if (rbResult != MA_SUCCESS) {
                fprintf(stderr, "Failed to initialize PCM ring buffer: %d\n", rbResult);
                return -1;
        }

        return 0;
}

int create_audio_device(
    UserData *user_data,
    AudioData *audio_data,
    ma_device *device,
    ma_context *context,
    decoder_getter_func get_decoder,
    ma_data_callback_proc callback)
{
        PlaybackState *ps = get_playback_state();

        ma_result result = init_first_datasource(user_data);

        audio_data->chunk_frames = audio_data->sample_rate / 10;

        if (result != MA_SUCCESS) {
                if (!has_error_message())
                        set_error_message("Failed to initialize file.");
                return -1;
        }

        void *decoder = get_decoder();
        if (!decoder)
                return -1;

        init_ring_buffer(audio_data);

        result = init_playback_device(
            context,
            audio_data,
            device,
            callback,
            decoder);

        set_volume(get_current_volume());
        ps->notifyPlaying = true;

        return result;
}

int pb_switch_audio_implementation(void)
{
        AppState *state = get_app_state();

        if (audio_data.end_of_list_reached) {
                pb_set_EOF_handled();
                set_current_implementation_type(NONE);
                return 0;
        }

        audio_data.pUserData->current_song_data = (audio_data.currentFileIndex == 0) ? audio_data.pUserData->songdataA : audio_data.pUserData->songdataB;
        if (!audio_data.pUserData->current_song_data) {
                pb_set_EOF_handled();
                return 0;
        }

        char *file_path = strdup(audio_data.pUserData->current_song_data->file_path);
        if (!valid_file_path(file_path)) {
                free(file_path);
                set_EOF_reached();
                return -1;
        }

        const CodecOps *ops = find_codec_ops(file_path);
        if (!ops) {
                free(file_path);
                return -1;
        }

        if (ops->implType == BUILTIN)
                handle_builtin_avg_bit_rate(&audio_data, audio_data.pUserData->current_song_data, file_path);

        int result = handle_codec(file_path, *ops, &audio_data, state, &context);
        free(file_path);

        if (result < 0) {
                set_current_implementation_type(NONE);
                set_impl_switch_not_reached();
                set_EOF_reached();
                return -1;
        }

        pb_set_EOF_handled();
        return 0;
}

bool is_context_initialized(void)
{
        return context_initialized;
}

void cleanup_audio_context(void)
{
        if (context_initialized) {
                ma_context_uninit(&context);
                context_initialized = false;
        }
}

int pb_create_audio_device(void)
{
        PlaybackState *ps = get_playback_state();

        pb_sound_shutdown();

        ma_context_init(NULL, 0, NULL, &context);
        context_initialized = true;

        if (pb_switch_audio_implementation() >= 0) {
                ps->notifySwitch = true;
        } else {
                return -1;
        }

        return 0;
}
