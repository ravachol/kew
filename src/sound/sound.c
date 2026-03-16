#include "ops/playback_ops.h"
#include "sound/sound_facade.h"
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

#include "loader/song_loader.h"

#include "playback.h"
#ifdef USE_FAAD
#include "m4a.h"
#endif
#include "audio_file_info.h"
#include "audiobuffer.h"
#include "audiotypes.h"
#include "decoders.h"
#include "volume.h"

#include "utils/file.h"
#include "utils/utils.h"

#include <miniaudio.h>

#include <fcntl.h>
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
static pthread_mutex_t switch_mutex = PTHREAD_MUTEX_INITIALIZER;

sound_system_t *sound_s = NULL;

sound_playback_repeat_state_t repeat_state = SOUND_STATE_REPEAT_OFF;

ma_pcm_rb pcm_rb;
pthread_mutex_t rb_mutex;
pthread_cond_t rb_cond;

int create_audio_device(
    void *user_data,
    sound_system_t *sound,
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
                ma_pcm_rb_reset(&pcm_rb);
                atomic_store(&sound_s->buffer_ready, 0);
                atomic_store(&sound_s->decode_finished, false);
        }

        resume_playback();

        return result;
}

static bool perform_seek_if_requested(ma_decoder *decoder)
{
        if (!is_seek_requested())
                return true;

        set_seek_requested(false);

        ma_uint64 totalFrames = sound_s->totalFrames;
        double seek_percent = get_seek_percentage();
        if (seek_percent > 100.0)
                seek_percent = 100.0;

        ma_uint64 targetFrame =
            (ma_uint64)((totalFrames - 1) * seek_percent / 100.0);
        if (targetFrame >= totalFrames)
                targetFrame = totalFrames - 1;

        const CodecOps *ops = get_codec_ops(get_current_decoder_decoder_type());

        return execute_seek(decoder, targetFrame, ops);
}

double db_to_linear(double db)
{
        return pow(10.0, db / 20.0);
}

bool is_valid_gain(double gain)
{
        return gain > -50.0 && gain < 50.0 && !isnan(gain) && isfinite(gain) && gain != 0;
}

static bool compute_replay_gain(double *out_gain_db)
{
        bool result = false;
        double gain_db = 0.0;

        SongData *song = sound_get_current_song_data();

        if (song != NULL && song->magic == SONG_MAGIC && song->metadata != NULL) {
                double track_gain = song->metadata->replaygainTrack;
                double album_gain = song->metadata->replaygainAlbum;

                int checkFirst = sound_s->replay_gain_check_first;

                if (checkFirst == 0 && is_valid_gain(track_gain)) {
                        gain_db = track_gain;
                        result = true;
                } else if (checkFirst == 1 && is_valid_gain(album_gain)) {
                        gain_db = album_gain;
                        result = true;
                } else if (checkFirst == 1 && is_valid_gain(track_gain)) {
                        gain_db = track_gain;
                        result = true;
                } else if (checkFirst == 0 && is_valid_gain(album_gain)) {
                        gain_db = track_gain;
                        result = true;
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

void reset_ring_buffer(void)
{
        stop_playback();

        ma_pcm_rb_reset(&pcm_rb);

        resume_playback();
}

void set_switch_files(bool value)
{
        atomic_store(&sound_s->switch_files, value);
}

void activate_switch(void)
{
        set_skip_to_next(false);

        if (!pb_is_repeat_enabled()) {
                pthread_mutex_lock(&switch_mutex);

                bool using_slot_A = atomic_load(&sound_s->using_song_slot_A); // fetch

                using_slot_A = !using_slot_A; // invert

                atomic_store(&sound_s->using_song_slot_A, using_slot_A); // store

                pthread_mutex_unlock(&switch_mutex);
        }

        set_switch_files(true);
}

void execute_switch(void)
{
        set_switch_files(false);
        switch_decoder_index();

        sound_s->totalFrames = 0;
        sound_s->currentPCMFrame = 0;

        set_seek_elapsed(0.0);

        set_EOF_reached();
}

// Main decoding loop, produces data to the miniaudio ringbuffer
void *decode_loop(void *arg)
{
        sound_system_t *sound = (sound_system_t *)arg;

        float temp[sound->chunk_frames * sound->channels];

        AppState *state = get_app_state();
        if (!state)
                goto done;

        while (atomic_load(&sound->decode_thread_running)) {

                // Activate switch
                if (atomic_load(&sound->pending_switch)) {

                        atomic_store(&sound->pending_switch, false);

                        while (pthread_mutex_trylock(&state->data_source_mutex) != 0) {
                                if (!atomic_load(&sound->decode_thread_running))
                                        goto done;
                                c_sleep(1);
                        }

                        atomic_store(&sound->decode_finished, false);

                        activate_switch();

                        pthread_mutex_unlock(&state->data_source_mutex);
                        continue;
                }

                // Execute switch
                if (atomic_load(&sound->switch_files)) {

                        pthread_mutex_lock(&state->data_source_mutex);

                        atomic_store(&sound->buffer_ready, 0);
                        atomic_store(&sound->decode_finished, false);

                        execute_switch();

                        pthread_mutex_unlock(&state->data_source_mutex);
                        continue;
                }

                if (get_current_decoder_type() != get_current_decoder_decoder_type()) {
                        pthread_mutex_unlock(&state->data_source_mutex);
                        goto done;
                }

                // Wait for ring buffer to have enough space to fill
                pthread_mutex_lock(&rb_mutex);
                while (ma_pcm_rb_available_write(&pcm_rb) < sound->chunk_frames / 4 &&
                       atomic_load(&sound->decode_thread_running) &&
                       !atomic_load(&sound->switch_files) &&
                       !atomic_load(&sound->pending_switch)) {

                        struct timespec ts;
                        clock_gettime(CLOCK_REALTIME, &ts);
                        ts.tv_nsec += 10 * 1000000;
                        if (ts.tv_nsec >= 1000000000) {
                                ts.tv_sec += 1;
                                ts.tv_nsec -= 1000000000;
                        }

                        atomic_store(&sound->buffer_ready, 1);

                        pthread_cond_timedwait(&rb_cond, &rb_mutex, &ts);
                }

                // Get a writable chunk
                ma_uint32 writable = ma_pcm_rb_available_write(&pcm_rb);
                pthread_mutex_unlock(&rb_mutex);

                // Pause
                while (pb_is_paused() && atomic_load(&sound->decode_thread_running)) {
                        c_sleep(10);
                        continue;
                }

                if (!atomic_load(&sound->decode_thread_running))
                        goto done;

                if (writable == 0)
                        continue;

                ma_uint32 frames_to_decode = writable;
                if (frames_to_decode > sound->chunk_frames)
                        frames_to_decode = sound->chunk_frames;

                // Lock decoder
                while (pthread_mutex_trylock(&state->data_source_mutex) != 0) {
                        if (!atomic_load(&sound->decode_thread_running))
                                goto done;
                        c_sleep(1);
                }

                if (is_decoder_type_switch_reached()) {
                        pthread_mutex_unlock(&state->data_source_mutex);
                        goto done;
                }

                const CodecOps *ops = get_codec_ops(get_current_decoder_decoder_type());
                void *decoder = get_current_decoder();

                if (!decoder || pb_is_EOF_reached()) {
                        pthread_mutex_unlock(&state->data_source_mutex);
                        c_sleep(1);
                        continue;
                }

                if (sound_s->totalFrames == 0)
                        ma_data_source_get_length_in_pcm_frames(decoder, &sound_s->totalFrames);

                if (!perform_seek_if_requested(decoder)) {
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
                if (get_current_decoder_decoder_type() == BUILTIN &&
                    compute_replay_gain(&gain_db)) {

                        apply_gain_to_interleaved_frames(
                            temp,
                            sound->format,
                            frames_to_read,
                            sound->channels,
                            db_to_linear(gain_db));
                }

                if (!atomic_load(&sound_s->pending_switch) &&
                    !atomic_load(&sound_s->decode_finished) &&
                    should_switch(frames_to_read, result, (ma_uint64)cursor)) {

                        if (cursor != lastCursor) {
                                atomic_store(&sound_s->pending_switch, true);
                                reset_ring_buffer();
                        } else {
                                atomic_store(&sound_s->decode_finished, true);

                                int sent = atomic_load(&sound_s->track_frames_sent) + ma_pcm_rb_available_read(&pcm_rb);

                                atomic_store(&sound_s->track_end_frame, sent);
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

                                if (!atomic_load(&sound_s->decode_thread_running))
                                        goto done;

                                ma_uint32 framesToWrite = framesRemaining;
                                void *pWriteBuffer = NULL;

                                ma_result wr = ma_pcm_rb_acquire_write(
                                    &pcm_rb,
                                    &framesToWrite,
                                    &pWriteBuffer);

                                if (wr != MA_SUCCESS || framesToWrite == 0 || pWriteBuffer == NULL)
                                        break;

                                MA_COPY_MEMORY(
                                    pWriteBuffer,
                                    (float *)temp + framesWritten * sound->channels,
                                    framesToWrite * sound->channels * sizeof(float));

                                ma_pcm_rb_commit_write(&pcm_rb, framesToWrite);

                                framesWritten += framesToWrite;
                                framesRemaining -= framesToWrite;

                                if (!atomic_load(&sound_s->buffer_ready))
                                        atomic_store(&sound_s->buffer_ready, 1);
                        }
                }
        }

done:
        sound_s->decode_thread_active = false;
        return NULL;
}

int underrun_count = 0;

// Audio callback, consumes data from the miniaudio ringbuffer, and feeds the output device
void on_audio_frames(ma_device *device,
                     void *pOutput,
                     const void *input,
                     ma_uint32 frameCount)
{
        (void)device;
        (void)input;

        if (!atomic_load(&sound_s->buffer_ready)) {
                memset(pOutput, 0, frameCount * sound_s->channels * sizeof(float));
                return;
        }

        const ma_uint32 frameSize = sound_s->channels * sizeof(float);
        ma_uint32 framesRemaining = frameCount;
        ma_uint32 totalFramesRead = 0;

        while (framesRemaining > 0) {

                ma_uint32 framesToRead = framesRemaining;
                void *pReadBuffer = NULL;

                ma_result result =
                    ma_pcm_rb_acquire_read(&pcm_rb,
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

                ma_pcm_rb_commit_read(&pcm_rb, framesToRead);

                totalFramesRead += framesToRead;
                framesRemaining -= framesToRead;

                atomic_fetch_add(&sound_s->track_frames_sent, framesToRead);

                if (atomic_load(&sound_s->decode_finished)) {
                        uint64_t played =
                            atomic_load(&sound_s->track_frames_sent);

                        uint64_t boundary =
                            atomic_load(&sound_s->track_end_frame);

                        if (played >= boundary) {
                                atomic_store(&sound_s->pending_switch, true);
                        }
                }

                /* Wake decode thread when ring buffer has meaningful space available. */
                ma_uint32 spaceAvailable = ma_pcm_rb_available_write(&pcm_rb);

                if (spaceAvailable >= sound_s->chunk_frames / 4) {
                        pthread_mutex_lock(&rb_mutex);
                        pthread_cond_signal(&rb_cond);
                        pthread_mutex_unlock(&rb_mutex);
                }
        }

        set_audio_buffer(pOutput,
                         totalFramesRead,
                         sound_s->sample_rate,
                         sound_s->channels,
                         sound_s->format);
}

int handle_codec(
    const char *file_path,
    CodecOps ops,
    sound_system_t *sound,
    AppState *state,
    ma_context *context)
{
        ma_uint32 sample_rate, channels, nSampleRate, nChannels;
        ma_format format, nFormat;
        ma_channel channel_map[MA_MAX_CHANNELS], nChannelMap[MA_MAX_CHANNELS];
        enum decoder_type_t current_implementation = get_current_decoder_type();

        int avg_bit_rate = 0;

#ifdef USE_FAAD
        k_m4adec_filetype file_type = 0;

        if (ops.decoder_type == M4A) {
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
                sameFormat = (ops.decoder_type == get_current_decoder_decoder_type() && format == nFormat && channels == nChannels &&
                              sample_rate == nSampleRate);
#ifdef USE_FAAD
                if (ops.decoder_type == M4A && decoder != NULL) {
                        sameFormat = sameFormat &&
                                     (((ma_m4a *)decoder)->file_type == file_type) &&
                                     (((ma_m4a *)decoder)->file_type != k_rawAAC);
                }
#endif
        }

        SongData *songdata = sound_get_current_song_data();

        // Avg bitrate assignment
        if (songdata) {
                if (ops.decoder_type == M4A)
                        songdata->avg_bit_rate = sound_s->avg_bit_rate = avg_bit_rate;
                else
                        songdata->avg_bit_rate =
                            sound_s->avg_bit_rate =
                                calc_avg_bit_rate(songdata->duration, file_path);
        } else {
                sound_s->avg_bit_rate = 0;
        }

        if (repeat_state == SOUND_STATE_REPEAT ||
            !(sameFormat && current_implementation == ops.decoder_type)) {
                set_decoder_type_switch_reached();

                pthread_mutex_lock(&(state->data_source_mutex));

                set_current_decoder_type(ops.decoder_type);

                cleanup_playback_device(); // FIXME should be after lock?

                reset_decoders();
                reset_audio_buffer();

                sound->sample_rate = sample_rate;

                int result;

                result = create_audio_device(
                    NULL,
                    sound,
                    get_device(),
                    context,
                    get_current_decoder,
                    on_audio_frames);

                if (result < 0) {
                        set_current_decoder_type(NONE);
                        set_decoder_type_switch_not_reached();
                        set_EOF_reached();
                        pthread_mutex_unlock(&(state->data_source_mutex));
                        return -1;
                }

                // Reset buffer state for new song
                atomic_store(&sound_s->buffer_ready, 0);
                atomic_store(&sound_s->decode_thread_running, true);
                sound_s->decode_thread_active = true;
                pthread_create(&sound_s->decode_thread,
                               NULL,
                               decode_loop,
                               sound_s);

                pthread_mutex_unlock(&(state->data_source_mutex));
                set_decoder_type_switch_not_reached();
        }

        return 0;
}

static void handle_builtin_avg_bit_rate(SongData *song_data, const char *file_path)
{
        if (path_ends_with(file_path, ".mp3") && song_data) {
                int avg_bit_rate = calc_avg_bit_rate(song_data->duration, file_path);
                if (avg_bit_rate > 320)
                        avg_bit_rate = 320;
                song_data->avg_bit_rate = sound_s->avg_bit_rate = avg_bit_rate;
        } else {
                sound_s->avg_bit_rate = 0;
        }
}

static int init_audio_data_from_codec_decoder(const CodecOps *ops, void *decoder, sound_system_t *sound)
{
        if (!ops || !decoder || !sound_s)
                return -1;

        ma_channel channel_map[MA_MAX_CHANNELS];

        switch (ops->decoder_type) {
        case BUILTIN: {
                ma_decoder *d = (ma_decoder *)decoder;
                sound->format = ma_format_f32;
                sound->channels = d->outputChannels;
                sound->sample_rate = d->outputSampleRate;
                ma_data_source_get_length_in_pcm_frames((ma_data_source *)d, &sound->totalFrames);
                break;
        }

        case OPUS: {
                ma_libopus *d = (ma_libopus *)decoder;
                ma_libopus_ds_get_data_format(d, &sound->format, &sound->channels,
                                              &sound->sample_rate, channel_map, MA_MAX_CHANNELS);
                ma_data_source_get_length_in_pcm_frames((ma_data_source *)d, &sound->totalFrames);

                ((ma_data_source_base *)d)->pCurrent = d;
                d->pReadSeekTellUserData = sound;
                break;
        }

        case VORBIS: {
                ma_libvorbis *d = (ma_libvorbis *)decoder;
                ma_libvorbis_ds_get_data_format(d, &sound->format, &sound->channels,
                                                &sound->sample_rate, channel_map, MA_MAX_CHANNELS);
                ma_data_source_get_length_in_pcm_frames((ma_data_source *)d, &sound->totalFrames);

                ((ma_data_source_base *)d)->pCurrent = d;
                d->pReadSeekTellUserData = sound;
                break;
        }

        case WEBM: {
                ma_webm *d = (ma_webm *)decoder;
                ma_webm_ds_get_data_format(d, &sound->format, &sound->channels,
                                           &sound->sample_rate, channel_map, MA_MAX_CHANNELS);
                ma_data_source_get_length_in_pcm_frames((ma_data_source *)d, &sound->totalFrames);

                ((ma_data_source_base *)d)->pCurrent = d;
                d->pReadSeekTellUserData = sound;
                break;
        }

#ifdef USE_FAAD
        case M4A: {
                ma_m4a *d = (ma_m4a *)decoder;
                m4a_decoder_ds_get_data_format(d, &sound->format, &sound->channels,
                                               &sound->sample_rate, channel_map, MA_MAX_CHANNELS);
                ma_data_source_get_length_in_pcm_frames((ma_data_source *)d, &sound->totalFrames);

                ((ma_data_source_base *)d)->pCurrent = d;
                d->pReadSeekTellUserData = sound;
                break;
        }
#endif

        default:
                return -1;
        }

        sound->format = ma_format_f32; // hard-coded. float is used throughout

        return 0;
}

int init_first_datasource(sound_system_t *sound)
{
        if (!sound)
                return MA_ERROR;

        SongData *song_data = (sound->using_song_slot_A) ? sound->songdataA : sound->songdataB;

        if (!song_data)
                return MA_ERROR;

        const char *file_path = song_data->file_path;
        if (!file_path)
                return MA_ERROR;

        const CodecOps *ops = find_codec_ops(file_path);
        if (!ops)
                return MA_ERROR;

        int result = prepare_next_decoder(file_path, song_data, ops);
        if (result < 0)
                return -1;

        void *decoder = get_first_decoder();
        if (!decoder)
                return -1;

        result = init_audio_data_from_codec_decoder(ops, decoder, sound);
        if (result < 0)
                return -1;

        // BUILTIN MP3 special handling
        if (ops->decoder_type == BUILTIN) {
                handle_builtin_avg_bit_rate(song_data, file_path);
        }

        return MA_SUCCESS;
}

void safe_ringbuffer_reset(ma_pcm_rb *pcm_rb)
{
        if (pcm_rb->rb.ownsBuffer) {
                ma_pcm_rb_uninit(pcm_rb);
        }
        memset(pcm_rb, 0, sizeof(*pcm_rb)); // reset the entire struct safely
}

int init_ring_buffer(sound_system_t *sound)
{
        // 3 seconds of buffer regardless of sample rate
        ma_uint32 rbFrames = sound->sample_rate * 3;

        atomic_store(&sound->buffer_ready, 0);
        atomic_store(&sound->decode_finished, false);
        atomic_store(&sound->pending_switch, false);

        safe_ringbuffer_reset(&pcm_rb);

        pcm_rb.rb.pBuffer = NULL;
        pcm_rb.rb.ownsBuffer = false;

        ma_result rbResult = ma_pcm_rb_init(sound->format,
                                            sound->channels,
                                            rbFrames,
                                            NULL,
                                            NULL,
                                            &pcm_rb);
        if (rbResult != MA_SUCCESS) {
                fprintf(stderr, "Failed to initialize PCM ring buffer: %d\n", rbResult);
                return -1;
        }

        return 0;
}

int create_audio_device(
    void *user_data,
    sound_system_t *sound,
    ma_device *device,
    ma_context *context,
    decoder_getter_func get_decoder,
    ma_data_callback_proc callback)
{
        PlaybackState *ps = get_playback_state();

        ma_result result = init_first_datasource(sound_s);

        sound_s->chunk_frames = sound_s->sample_rate / 10;

        if (result != MA_SUCCESS)
                return -1;

        void *decoder = get_decoder();
        if (!decoder)
                return -1;

        init_ring_buffer(sound_s);

        result = init_playback_device(
            context,
            sound,
            device,
            callback,
            user_data);
        set_current_volume(get_current_volume());
        ps->notifyPlaying = true;

        return result;
}

int sound_switch_decoder_type(void)
{
        AppState *state = get_app_state();

        if (sound_system_is_end_of_list_reached(sound_s)) {
                pb_set_EOF_handled();
                set_current_decoder_type(NONE);
                return 0;
        }

        SongData *current_song = sound_get_current_song_data();

        if (!current_song) {
                pb_set_EOF_handled();
                return 0;
        }

        char *file_path = strdup(current_song->file_path);
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

        if (ops->decoder_type == BUILTIN)
                handle_builtin_avg_bit_rate(current_song, file_path);

        int result = handle_codec(file_path, *ops, sound_s, state, &context);
        free(file_path);

        if (result < 0) {
                set_current_decoder_type(NONE);
                set_decoder_type_switch_not_reached();
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

int sound_create_audio_device(void)
{
        PlaybackState *ps = get_playback_state();

        cleanup_playback_device();

        if (!is_context_initialized()) {
                ma_context_init(NULL, 0, NULL, &context);
                context_initialized = true;
        }

        if (sound_switch_decoder_type() >= 0) {
                ps->notifySwitch = true;
        } else {
                return -1;
        }

        return 0;
}

SongData *sound_get_current_song_data(void)
{
        return (atomic_load(&sound_s->using_song_slot_A)) ? sound_s->songdataA : sound_s->songdataB;
}

int load_decoder(SongData *song_data, bool *song_data_deleted)
{
        LoaderData *loader_data = get_loader_data();

        int result = 0;

        if (song_data != NULL) {
                *song_data_deleted = false;

                // This should only be done for the second song, as
                // switch_audio_implementation() handles the first one
                if (!loader_data->loadingFirstDecoder) {
                        const CodecOps *ops = find_codec_ops(song_data->file_path);
                        if (!ops)
                                result = -1;
                        else
                                result = prepare_next_decoder(song_data->file_path, song_data, ops);
                }
        }
        return result;
}

void set_song_data(bool slotA, SongData *songdata)
{
        if (!sound_s)
                return;

        if (slotA)
                sound_s->songdataA = songdata;
        else
                sound_s->songdataB = songdata;
}

int assign_loaded_data(void)
{
        LoaderData *loader_data = get_loader_data();

        int result = 0;

        if (loader_data->loadInSlotA) {

                set_song_data(loader_data->loadInSlotA, loader_data->songdataA);

                result = load_decoder(loader_data->songdataA,
                                      &(loader_data->songdataADeleted));
        } else {
                set_song_data(loader_data->loadInSlotA, loader_data->songdataB);

                result = load_decoder(loader_data->songdataB,
                                      &(loader_data->songdataBDeleted));
        }

        return result;
}

bool sound_is_using_slot_A()
{
        if (!sound_s)
                return true;

        return atomic_load(&sound_s->using_song_slot_A);
}

void sound_set_using_slot_A(bool value)
{
        if (!sound_s)
                return;

        atomic_store(&sound_s->using_song_slot_A, value);
}

int sound_get_bit_depth(ma_format format)
{

        if (format == ma_format_unknown)
                return -1;

        int bit_depth = 32;

        switch (format) {
        case ma_format_u8:
                bit_depth = 8;
                break;

        case ma_format_s16:
                bit_depth = 16;
                break;

        case ma_format_s24:
                bit_depth = 24;
                break;

        case ma_format_f32:
        case ma_format_s32:
                bit_depth = 32;
                break;
        default:
                break;
        }

        return bit_depth;
}

void *song_data_reader_thread(void *arg)
{
        PlaybackState *ps = (PlaybackState *)arg;
        LoaderData *loader_data = get_loader_data();

        char filepath[PATH_MAX];
        c_strcpy(filepath, loader_data->file_path, sizeof(filepath));

        SongData *songdata = exists_file(filepath) >= 0 ? load_song_data(filepath) : NULL;

        if (loader_data->replaceNextSong) {
                loader_data->loadInSlotA = !sound_is_using_slot_A();
        }

        if (loader_data->loadingFirstDecoder) {
                loader_data->loadInSlotA = true;
                sound_set_using_slot_A(true);
        }

        if (loader_data->loadInSlotA) {

                song_loader_assign_slot_A(songdata);

        } else {

                song_loader_assign_slot_B(songdata);
        }

        int result = assign_loaded_data();

        loader_data->loadInSlotA = !loader_data->loadInSlotA;

        if (result < 0)
                songdata->hasErrors = true;

        if (songdata == NULL || songdata->hasErrors) {
                ps->songHasErrors = true;
                ps->clearingErrors = true;
                set_next_song(NULL);
        } else {
                ps->songHasErrors = false;
                ps->clearingErrors = false;
                set_next_song(get_try_next_song());
                set_try_next_song(NULL);
        }

        ps->loadedNextSong = true;
        ps->skipping = false;
        ps->songLoading = false;

        return NULL;
}

void sound_load_song(const char *file_path, int is_first_decoder, int replace_next_song)
{
        PlaybackState *ps = get_playback_state();
        LoaderData *loader_data = get_loader_data();

        if (find_codec_ops(file_path) == NULL)
        {
                ps->songHasErrors = 1;
                return;
        }

        c_strcpy(loader_data->file_path, file_path,
                 sizeof(loader_data->file_path));

        loader_data->loadingFirstDecoder = is_first_decoder;
        loader_data->replaceNextSong = replace_next_song;

        pthread_t loading_thread;
        pthread_create(&loading_thread, NULL, song_data_reader_thread, ps);
}

void sound_switch_track_immediate(void)
{
        activate_switch();
}

void sound_shutdown(void)
{
        if (is_context_initialized()) {
                cleanup_playback_device();
                cleanup_audio_context();
        }
}

void sound_ringbuffer_cleanup(void)
{
        if (pcm_rb.rb.ownsBuffer) {
                ma_pcm_rb_uninit(&pcm_rb);
        }
        memset(&pcm_rb, 0, sizeof(pcm_rb));
}

void sound_wake_up(void)
{
        pthread_mutex_lock(&rb_mutex);
        pthread_cond_broadcast(&rb_cond);
        pthread_mutex_unlock(&rb_mutex);
}
