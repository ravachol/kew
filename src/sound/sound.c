#include "ops/playback_ops.h"
#include "sound/sound_facade.h"
#define MA_EXPERIMENTAL__DATA_LOOPING_AND_CHAINING
#define MA_NO_ENGINE
#define MA_ENABLE_PIPEWIRE
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
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(__linux__) || defined(__FreeBSD__) || defined(__ANDROID__)
#include <sys/resource.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static ma_context context;
static bool context_initialized = false;
long long lastCursor = 0;
static pthread_mutex_t switch_mutex = PTHREAD_MUTEX_INITIALIZER;

sound_system_t *sound_s = NULL;

sound_playback_repeat_state_t repeat_state = SOUND_STATE_REPEAT_OFF;

ma_pcm_rb pcm_rb;

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

static bool execute_seek(sound_system_t *sound, ma_decoder *decoder, ma_uint64 targetFrame, const CodecOps *ops)
{
        stop_playback();

        ma_result result = ops->seek_to_pcm_frame(decoder, targetFrame, 0);

        if (result == MA_SUCCESS) {
                atomic_store(&sound->buffer_ready, 0);
                ma_pcm_rb_reset(&pcm_rb);
                atomic_store(&sound->decode_finished, false);
        }

        resume_playback();

        return result;
}

static bool perform_seek_if_requested(sound_system_t *sound, ma_decoder *decoder)
{
        if (!is_seek_requested())
                return true;

        set_seek_requested(false);

        ma_uint64 totalFrames = 0;

        ma_data_source_get_length_in_pcm_frames(decoder, &totalFrames);

        double seek_percent = get_seek_percentage();
        if (seek_percent > 100.0)
                seek_percent = 100.0;

        ma_uint64 targetFrame =
            (ma_uint64)((totalFrames - 1) * seek_percent / 100.0);
        if (targetFrame >= totalFrames)
                targetFrame = totalFrames - 1;

        const CodecOps *ops = get_codec_ops(get_current_decoder_decoder_type());

        return execute_seek(sound, decoder, targetFrame, ops);
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
        double gain_db = 0.0f;

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
                        gain_db = album_gain;
                        result = true;
                }
        }

        if (result) {
                *out_gain_db = gain_db;
        }

        return result;
}

static inline void
apply_gain_f32(float *restrict samples,
               ma_uint64 total_samples,
               float gain)
{
        for (ma_uint64 i = 0; i < total_samples; i++) {
                samples[i] *= gain;
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

void set_replay_gain(sound_system_t *sound)
{
        sound->gain_linear = 1.0f;

        double gain_db = 0.0;
        if (compute_replay_gain(&gain_db)) {
                float g = (float)db_to_linear(gain_db);

                if (g < 0.0001f)
                        g = 0.0001f;
                if (g > 10.0f)
                        g = 10.0f;

                sound->gain_linear = g;
        }
}

void execute_switch(sound_system_t *sound)
{
        atomic_store(&sound->buffer_ready, 0);

        set_switch_files(false);
        switch_decoder_index();

        sound->currentPCMFrame = 0;
        lastCursor = 0;

        set_replay_gain(sound);

        set_seek_elapsed(0.0);

        set_EOF_reached();
}

void set_decode_thread_priority(pthread_t thread)
{
#if defined(__linux__) || defined(__FreeBSD__)
        struct sched_param param = {.sched_priority = 1};
        int ret = pthread_setschedparam(thread, SCHED_RR, &param);
        if (ret != 0) {
                fprintf(stderr, "pthread_setschedparam failed: %s\n", strerror(ret));
                ret = setpriority(PRIO_PROCESS, 0, -5);
                fprintf(stderr, "setpriority returned: %d (errno: %s)\n", ret, strerror(errno));
        }
#elif defined(__APPLE__)
        (void)thread;
        pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#elif defined(__ANDROID__)
        (void)thread;
        setpriority(PRIO_PROCESS, 0, -8);
#endif
}

// Main decoding loop, produces data to the miniaudio ringbuffer
void *decode_loop(void *arg)
{
        sound_system_t *sound = (sound_system_t *)arg;

        size_t tempSize = sound->chunk_frames * sound->channels * sizeof(float);
        float *temp = NULL;

        if (posix_memalign((void **)&temp, 64, tempSize) != 0) {
                sound->decode_thread_running = false;
                return NULL;
        }

        lastCursor = 0;
        set_decode_thread_priority(pthread_self());

        while (atomic_load(&sound->decode_thread_running)) {

                // Handle pending switch
                if (atomic_load_explicit(&sound->pending_switch, memory_order_acquire)) {
                        atomic_store_explicit(&sound->pending_switch, false, memory_order_release);
                        atomic_store_explicit(&sound->decode_finished, false, memory_order_release);
                        activate_switch();
                }

                // Handle file switch
                if (atomic_load_explicit(&sound->switch_files, memory_order_acquire)) {
                        atomic_store_explicit(&sound->decode_finished, false, memory_order_release);
                        execute_switch(sound);
                        continue;
                }

                // Decoder type switch reached
                if (get_current_decoder_type() != get_current_decoder_decoder_type() ||
                    is_decoder_type_switch_reached()) {
                        break;
                }

                ma_uint32 writable = 0;

                while (atomic_load(&sound->decode_thread_running)) {

                        atomic_thread_fence(memory_order_seq_cst);
                        writable = ma_pcm_rb_available_write(&pcm_rb);

                        if (writable > 0) {
                                atomic_store(&sound->buffer_ready, 1);
                                break;
                        }

                        struct timespec ts = {0, 1000000}; // 1ms
                        nanosleep(&ts, NULL);
                }

                // Handle pause
                while (pb_is_paused() && atomic_load(&sound->decode_thread_running)) {
                        c_sleep(10);
                        continue;
                }

                ma_uint32 frames_to_decode = (writable > sound->chunk_frames) ? sound->chunk_frames : writable;

                const CodecOps *ops = get_codec_ops(get_current_decoder_decoder_type());

                void *decoder = get_current_decoder();
                if (!decoder || !atomic_load(&sound->decode_thread_running) || pb_is_EOF_reached()) {
                        continue;
                }

                if (!perform_seek_if_requested(sound, decoder))
                        continue;

                // Decode
                ma_uint64 frames_to_read = 0;
                ma_result result = ma_data_source_read_pcm_frames(decoder, temp, frames_to_decode, &frames_to_read);

                long long cursor = 0;
                ops->get_cursor(decoder, &cursor);

                // Handle end-of-track switching
                if (!atomic_load(&sound->pending_switch) &&
                    !atomic_load(&sound->decode_finished) &&
                    should_switch(frames_to_read, result, (ma_uint64)cursor)) {

                        if (cursor != lastCursor) { // mid-song switch, clear buffer
                                atomic_store(&sound->pending_switch, true);
                                reset_ring_buffer();
                                continue;
                        } else { // end song, but play the remaining buffer

                                // Write the last frames before marking finished
                                if (frames_to_read > 0) {
                                        ma_uint32 framesRemaining = (ma_uint32)frames_to_read;
                                        ma_uint32 framesWritten = 0;
                                        while (framesRemaining > 0) {
                                                ma_uint32 framesToWrite = framesRemaining;
                                                void *pWriteBuffer = NULL;
                                                ma_result wr = ma_pcm_rb_acquire_write(&pcm_rb, &framesToWrite, &pWriteBuffer);
                                                if (wr != MA_SUCCESS || framesToWrite == 0) {
                                                        struct timespec ts = {0, 500000};
                                                        nanosleep(&ts, NULL);
                                                        continue;
                                                }
                                                memcpy(pWriteBuffer,
                                                       temp + framesWritten * sound->channels,
                                                       framesToWrite * sound->channels * sizeof(float));

                                                ma_pcm_rb_commit_write(&pcm_rb, framesToWrite);
                                                framesWritten += framesToWrite;
                                                framesRemaining -= framesToWrite;
                                        }
                                }

                                atomic_thread_fence(memory_order_seq_cst);
                                long long sent = atomic_load(&sound->track_frames_sent) +
                                                 (long long)ma_pcm_rb_available_read(&pcm_rb);
                                atomic_store_explicit(&sound->track_end_frame, sent, memory_order_release);
                                atomic_store_explicit(&sound->decode_finished, true, memory_order_release);
                                continue;
                        }
                }
                lastCursor = cursor;

                // Write decoded frames to ring buffer
                if (frames_to_read > 0) {
                        ma_uint32 framesRemaining = (ma_uint32)frames_to_read;
                        ma_uint32 framesWritten = 0;

                        while (framesRemaining > 0 && atomic_load(&sound->decode_thread_running)) {

                                // if (atomic_load_explicit(&sound->pending_switch, memory_order_acquire) ||
                                //     atomic_load_explicit(&sound->switch_files, memory_order_acquire)) {
                                //         break;
                                // }

                                ma_uint32 framesToWrite = framesRemaining;
                                void *pWriteBuffer = NULL;
                                ma_result wr = ma_pcm_rb_acquire_write(&pcm_rb, &framesToWrite, &pWriteBuffer);

                                if (wr != MA_SUCCESS || framesToWrite == 0 || pWriteBuffer == NULL)
                                        break;

                                MA_COPY_MEMORY(
                                    pWriteBuffer,
                                    (float *)temp + framesWritten * sound->channels,
                                    framesToWrite * sound->channels * sizeof(float));

                                ma_pcm_rb_commit_write(&pcm_rb, framesToWrite);

                                framesWritten += framesToWrite;
                                framesRemaining -= framesToWrite;

                                if (!atomic_load(&sound->buffer_ready))
                                        atomic_store(&sound->buffer_ready, 1);
                        }
                }
        }

        free(temp);
        atomic_store(&sound->decode_thread_running, false);
        return NULL;
}

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


        if (!sound_s->audio_thread_priority_set) {
                set_decode_thread_priority(pthread_self());
                sound_s->audio_thread_priority_set = MA_TRUE;
        }

        const ma_uint32 frameSize = sound_s->channels * sizeof(float);
        ma_uint32 framesRemaining = frameCount;
        ma_uint32 totalFramesRead = 0;
        uint8_t *writePtr = (uint8_t *)pOutput;
        bool boundary_handled = false;

        while (framesRemaining > 0) {
                ma_uint32 framesToRead = framesRemaining;
                void *pReadBuffer = NULL;

                ma_result result = ma_pcm_rb_acquire_read(&pcm_rb, &framesToRead, &pReadBuffer);

                if (result != MA_SUCCESS || framesToRead == 0 || pReadBuffer == NULL) {

                        memset((uint8_t *)pOutput + totalFramesRead * frameSize,
                               0,
                               framesRemaining * frameSize);

                        totalFramesRead += framesRemaining;
                        break;
                }

                // Success, copy data
                float gain = sound_s->gain_linear;

                // Load current played and boundary
                uint64_t played = atomic_load_explicit(&sound_s->track_frames_sent, memory_order_relaxed);
                uint64_t boundary = atomic_load_explicit(&sound_s->track_end_frame, memory_order_relaxed);

                // Determine how many frames to copy this iteration
                ma_uint32 framesToCopy = framesToRead;

                if (!boundary_handled &&
                    played + framesToCopy >= boundary &&
                    atomic_load_explicit(&sound_s->decode_finished, memory_order_acquire)) {

                        // Only copy up to the track boundary
                        framesToCopy = (ma_uint32)(boundary - played);
                        atomic_store_explicit(&sound_s->pending_switch, true, memory_order_relaxed);
                        boundary_handled = true;
                }
                // Copy frames
                size_t bytesToCopy = framesToCopy * frameSize;
                MA_COPY_MEMORY(writePtr, pReadBuffer, bytesToCopy);

                // Apply Replay Gain
                if (gain != 1.0f) {
                        ma_uint64 total = framesToCopy * sound_s->channels;
                        apply_gain_f32((float *)writePtr, total, gain);
                }

                ma_pcm_rb_commit_read(&pcm_rb, framesToCopy);

                writePtr += bytesToCopy;
                totalFramesRead += framesToCopy;
                framesRemaining -= framesToCopy;

                atomic_fetch_add_explicit(&sound_s->track_frames_sent, framesToCopy, memory_order_relaxed);
        }

        visualizer_ringbuffer_push(pOutput, frameCount, sound_s->channels);
}

int handle_codec(
    const char *file_path,
    CodecOps ops,
    sound_system_t *sound,
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
                set_decoder_type_switch_reached(true);

                set_current_decoder_type(ops.decoder_type);

                cleanup_playback_device();

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
                        set_decoder_type_switch_reached(false);
                        set_EOF_reached();
                        return -1;
                }

                set_decoder_type_switch_reached(false);

                atomic_store(&sound_s->decode_thread_running, true);

                pthread_create(&sound_s->decode_thread,
                               NULL,
                               decode_loop,
                               sound_s);
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
                break;
        }

        case OPUS: {
                ma_libopus *d = (ma_libopus *)decoder;
                ma_libopus_ds_get_data_format(d, &sound->format, &sound->channels,
                                              &sound->sample_rate, channel_map, MA_MAX_CHANNELS);

                ((ma_data_source_base *)d)->pCurrent = d;
                d->pReadSeekTellUserData = sound;
                break;
        }

        case VORBIS: {
                ma_libvorbis *d = (ma_libvorbis *)decoder;
                ma_libvorbis_ds_get_data_format(d, &sound->format, &sound->channels,
                                                &sound->sample_rate, channel_map, MA_MAX_CHANNELS);

                ((ma_data_source_base *)d)->pCurrent = d;
                d->pReadSeekTellUserData = sound;
                break;
        }

        case WEBM: {
                ma_webm *d = (ma_webm *)decoder;
                ma_webm_ds_get_data_format(d, &sound->format, &sound->channels,
                                           &sound->sample_rate, channel_map, MA_MAX_CHANNELS);

                ((ma_data_source_base *)d)->pCurrent = d;
                d->pReadSeekTellUserData = sound;
                break;
        }

#ifdef USE_FAAD
        case M4A: {
                ma_m4a *d = (ma_m4a *)decoder;
                m4a_decoder_ds_get_data_format(d, &sound->format, &sound->channels,
                                               &sound->sample_rate, channel_map, MA_MAX_CHANNELS);

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

        sound->currentPCMFrame = 0;
        sound->audio_thread_priority_set = MA_FALSE;

        set_replay_gain(sound);

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

        int result = handle_codec(file_path, *ops, sound_s, &context);
        free(file_path);

        if (result < 0) {
                set_current_decoder_type(NONE);
                set_decoder_type_switch_reached(false);
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

        pthread_mutex_lock(&(loader_data->mutex));

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

        pthread_mutex_unlock(&(loader_data->mutex));

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

        if (find_codec_ops(file_path) == NULL) {
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
