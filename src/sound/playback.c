
#include "common/common.h"

#include "decoders.h"
#include "audiobuffer.h"
#include "sound.h"
#ifdef USE_FAAD
#include "m4a.h"
#endif

#include <stdatomic.h>

static ma_device device = {0};
static bool device_initialized = false;

static bool paused = false;
static bool stopped = true;
static bool repeat_enabled = false;

static bool seek_requested = false;
static float seek_percent = 0.0;
static double seek_elapsed;

static bool skip_to_next = false;

static _Atomic bool EOF_reached = false;
static _Atomic bool switch_reached = false;

static pthread_mutex_t data_source_mutex = PTHREAD_MUTEX_INITIALIZER;

static enum AudioImplementation current_implementation = NONE;

static double seek_elapsed;

ma_uint64 last_cursor = 0;

static pthread_mutex_t switch_mutex = PTHREAD_MUTEX_INITIALIZER;

double get_seek_elapsed(void) { return seek_elapsed; }

void set_seek_elapsed(double value) { seek_elapsed = value; }

float get_seek_percentage(void) { return seek_percent; }

bool is_seek_requested(void) { return seek_requested; }

void set_seek_requested(bool value) { seek_requested = value; }

void seek_percentage(float percent)
{
        seek_percent = percent;
        seek_requested = true;
}

bool is_EOF_reached(void) { return atomic_load(&EOF_reached); }

void set_EOF_reached(void) { atomic_store(&EOF_reached, true); }

void set_EOF_handled(void) { atomic_store(&EOF_reached, false); }

bool is_paused(void) { return paused; }

void set_paused(bool val) { paused = val; }

bool is_stopped(void) { return stopped; }

void set_stopped(bool val) { stopped = val; }

bool is_repeat_enabled(void) { return repeat_enabled; }

void set_repeat_enabled(bool value) { repeat_enabled = value; }

bool is_playing(void) { return ma_device_is_started(&device); }

bool is_playback_done(void)
{
        if (is_EOF_reached())
        {
                return true;
        }
        else
        {
                return false;
        }
}

void stop_playback(void)
{
        AppState *state = get_app_state();

        if (ma_device_is_started(&device))
        {
                ma_device_stop(&device);
        }

        set_paused(false);
        set_stopped(true);

        if (state->currentView != TRACK_VIEW)
        {
                trigger_refresh();
        }
}

void sound_resume_playback(void)
{
        // If this was unpaused with no song loaded

        AppState *state = get_app_state();

        if (audio_data.restart)
        {
                audio_data.endOfListReached = false;
        }

        if (!ma_device_is_started(&device))
        {
                if (ma_device_start(&device) != MA_SUCCESS)
                {
                        create_audio_device();
                        ma_device_start(&device);
                }
        }

        set_paused(false);

        set_stopped(false);

        if (state->currentView != TRACK_VIEW)
        {
                trigger_refresh();
        }
}

void pause_playback(void)
{
        AppState *state = get_app_state();

        if (ma_device_is_started(&device))
        {
                ma_device_stop(&device);
        }

        set_paused(true);

        if (state->currentView != TRACK_VIEW)
        {
                trigger_refresh();
        }
}

void toggle_pause_playback(void)
{
        if (ma_device_is_started(&device))
        {
                pause_playback();
        }
        else if (is_paused() || is_stopped())
        {
                sound_resume_playback();
        }
}

int init_playback_device(ma_context *context, ma_format format, ma_uint32 channels, ma_uint32 sample_rate,
                       ma_device *device, ma_device_data_proc dataCallback, void *pUserData)
{
        ma_result result;

        ma_device_config deviceConfig =
            ma_device_config_init(ma_device_type_playback);

        deviceConfig.playback.format = format;
        deviceConfig.playback.channels = channels;
        deviceConfig.sampleRate = sample_rate;
        deviceConfig.dataCallback = dataCallback;
        deviceConfig.pUserData = pUserData;

        result = ma_device_init(context, &deviceConfig, device);
        if (result != MA_SUCCESS)
        {
                set_error_message("Failed to initialize miniaudio device.");
                return -1;
        }
        else
        {
                device_initialized = true;
        }

        result = ma_device_start(device);

        if (result != MA_SUCCESS)
        {
                set_error_message("Failed to start miniaudio device.");
                return -1;
        }

        set_paused(false);

        set_stopped(false);

        return 0;
}

void cleanup_playback_device(void)
{
        if (!device_initialized)
                return;

        // Stop device safely before uninitializing.
        ma_result result = ma_device_stop(&device);

        if (result != MA_SUCCESS)
        {
                fprintf(stderr, "Warning: ma_device_stop() failed: %d\n", result);
        }

        // Uninit the device. This will block until the audio thread stops.
        ma_device_uninit(&device);

        // Clear memory so we don’t accidentally reuse it.
        memset(&device, 0, sizeof(device));

        device_initialized = false;

        set_stopped(true);
}


void shutdown_android(void)
{
        // Avoid race condition when shutting down
        memset(&device, 0, sizeof(device));
}

void sound_shutdown()
{
        if (is_context_initialized())
        {
#ifdef __ANDROID__
        shutdown_android();
#else
        ma_device_uninit(&device);
        memset(&device, 0, sizeof(device));
        cleanup_audio_context();
#endif
        }
}

ma_device *get_device(void) { return &device; }

enum AudioImplementation get_current_implementation_type(void)
{
        return current_implementation;
}

void get_current_format_and_sample_rate(ma_format *format, ma_uint32 *sample_rate)
{
        *format = ma_format_unknown;

        if (get_current_implementation_type() == BUILTIN)
        {
                ma_decoder *decoder = get_current_builtin_decoder();

                if (decoder != NULL)
                        *format = decoder->outputFormat;
        }
        else if (get_current_implementation_type() == OPUS)
        {
                ma_libopus *decoder = get_current_opus_decoder();

                if (decoder != NULL)
                        *format = decoder->format;
        }
        else if (get_current_implementation_type() == VORBIS)
        {
                ma_libvorbis *decoder = get_current_vorbis_decoder();

                if (decoder != NULL)
                        *format = decoder->format;
        }
        else if (get_current_implementation_type() == WEBM)
        {
                ma_webm *decoder = get_current_webm_decoder();

                if (decoder != NULL)
                        *format = decoder->format;
        }
        else if (get_current_implementation_type() == M4A)
        {
#ifdef USE_FAAD
                m4a_decoder *decoder = get_current_m4a_decoder();

                if (decoder != NULL)
                        *format = decoder->format;
#endif
        }

        *sample_rate = get_audio_data()->sample_rate;
}

void execute_switch(AudioData *pAudioData)
{
        pAudioData->switchFiles = false;
        switch_decoder();

        if (pAudioData == NULL)
                return;

        pAudioData->pUserData->currentSongData =
            (pAudioData->currentFileIndex == 0)
                ? pAudioData->pUserData->songdataA
                : pAudioData->pUserData->songdataB;
        pAudioData->totalFrames = 0;
        pAudioData->currentPCMFrame = 0;

        set_seek_elapsed(0.0);

        set_EOF_reached();
}

bool is_impl_switch_reached(void)
{
        return atomic_load(&switch_reached) ? true : false;
}

void set_impl_switch_reached(void) { atomic_store(&switch_reached, true); }

void set_impl_switch_not_reached(void) { atomic_store(&switch_reached, false); }

void set_current_implementation_type(enum AudioImplementation value)
{
        current_implementation = value;
}

bool is_skip_to_next(void) { return skip_to_next; }

void set_skip_to_next(bool value) { skip_to_next = value; }

void activate_switch(AudioData *pAudioData)
{
        set_skip_to_next(false);

        if (!is_repeat_enabled())
        {
                pthread_mutex_lock(&switch_mutex);
                pAudioData->currentFileIndex =
                    1 - pAudioData->currentFileIndex; // Toggle between 0 and 1
                pthread_mutex_unlock(&switch_mutex);
        }

        pAudioData->switchFiles = true;
}

void clear_current_track(void)
{
        if (ma_device_is_started(&device))
        {
                // Stop the device (which stops playback)
                ma_device_stop(&device);
        }

        clear_decoder_chain();
        reset_all_decoders();
}

#ifdef USE_FAAD
void m4a_read_pcm_frames(ma_data_source *pDataSource, void *pFramesOut,
                         ma_uint64 frameCount, ma_uint64 *pFramesRead)
{
        m4a_decoder *m4a = (m4a_decoder *)pDataSource;
        AudioData *pAudioData = (AudioData *)m4a->pReadSeekTellUserData;
        ma_uint64 framesRead = 0;

        while (framesRead < frameCount)
        {
                if (is_impl_switch_reached())
                        return;

                if (pthread_mutex_trylock(&data_source_mutex) != 0)
                {
                        return;
                }

                // Check if a file switch is required
                if (pAudioData->switchFiles)
                {
                        execute_switch(pAudioData);
                        pthread_mutex_unlock(&data_source_mutex);
                        break; // Exit the loop after the file switch
                }

                if (get_current_implementation_type() != M4A && !is_skip_to_next())
                {
                        pthread_mutex_unlock(&data_source_mutex);
                        return;
                }

                m4a_decoder *decoder = get_current_m4a_decoder();

                if (pAudioData->totalFrames == 0)
                        ma_data_source_get_length_in_pcm_frames(
                            decoder, &(pAudioData->totalFrames));

                // Check if seeking is requested
                if (is_seek_requested())
                {
                        if (decoder && decoder->fileType != k_rawAAC)
                        {
                                ma_uint64 totalFrames = pAudioData->totalFrames;
                                ma_uint64 seek_percent = get_seek_percentage();

                                if (seek_percent >= 100.0)
                                        seek_percent = 100.0;

                                ma_uint64 targetFrame =
                                    (ma_uint64)((totalFrames - 1) *
                                                seek_percent / 100.0);

                                if (targetFrame >= totalFrames)
                                        targetFrame = totalFrames - 1;

                                // Set the read pointer for the decoder
                                ma_result seekResult =
                                    m4a_decoder_seek_to_pcm_frame(decoder,
                                                                  targetFrame);
                                if (seekResult != MA_SUCCESS)
                                {
                                        // Handle seek error
                                        set_seek_requested(false);
                                        pthread_mutex_unlock(&data_source_mutex);
                                        return;
                                }
                        }

                        set_seek_requested(false); // Reset seek flag
                }

                // Read from the current decoder
                ma_uint64 framesToRead = 0;
                ma_result result;
                ma_uint64 remainingFrames = frameCount - framesRead;
                m4a_decoder *first_decoder = get_first_m4a_decoder();
                ma_uint64 cursor = 0;

                if (first_decoder == NULL)
                {
                        pthread_mutex_unlock(&data_source_mutex);
                        return;
                }

                if (is_EOF_reached())
                {
                        pthread_mutex_unlock(&data_source_mutex);
                        return;
                }

                result = call_read_PCM_frames(
                    first_decoder, m4a->format, pFramesOut, framesRead,
                    pAudioData->channels, remainingFrames, &framesToRead);

                ma_data_source_get_cursor_in_pcm_frames(decoder, &cursor);

                if (((cursor != 0 && cursor == last_cursor) ||
                     framesToRead == 0 || is_skip_to_next() ||
                     result != MA_SUCCESS) &&
                    !is_EOF_reached())
                {
                        activate_switch(pAudioData);
                        pthread_mutex_unlock(&data_source_mutex);
                        continue;
                }

                last_cursor = cursor;

                framesRead += framesToRead;
                set_buffer_size(framesToRead);

                pthread_mutex_unlock(&data_source_mutex);
        }

        set_audio_buffer(pFramesOut, framesRead, pAudioData->sample_rate,
                       pAudioData->channels, pAudioData->format);

        if (pFramesRead != NULL)
        {
                *pFramesRead = framesRead;
        }
}

void m4a_on_audio_frames(ma_device *pDevice, void *pFramesOut,
                         const void *pFramesIn, ma_uint32 frameCount)
{
        AudioData *pDataSource = (AudioData *)pDevice->pUserData;
        ma_uint64 framesRead = 0;
        m4a_read_pcm_frames(&(pDataSource->base), pFramesOut, frameCount,
                            &framesRead);
        (void)pFramesIn;
}
#endif

void opus_read_pcm_frames(ma_data_source *pDataSource, void *pFramesOut,
                          ma_uint64 frameCount, ma_uint64 *pFramesRead)
{
        ma_libopus *opus = (ma_libopus *)pDataSource;
        AudioData *pAudioData = (AudioData *)opus->pReadSeekTellUserData;

        ma_uint64 framesRead = 0;

        while (framesRead < frameCount)
        {
                if (is_impl_switch_reached())
                        return;

                if (pthread_mutex_trylock(&data_source_mutex) != 0)
                {
                        return;
                }

                // Check if a file switch is required
                if (pAudioData->switchFiles)
                {
                        execute_switch(pAudioData);
                        pthread_mutex_unlock(&data_source_mutex);
                        break; // Exit the loop after the file switch
                }

                if (get_current_implementation_type() != OPUS && !is_skip_to_next())
                {
                        pthread_mutex_unlock(&data_source_mutex);
                        return;
                }

                ma_libopus *decoder = get_current_opus_decoder();

                if (pAudioData->totalFrames == 0)
                        ma_data_source_get_length_in_pcm_frames(
                            decoder, &(pAudioData->totalFrames));

                // Check if seeking is requested
                if (is_seek_requested())
                {
                        ma_uint64 totalFrames = 0;
                        ma_libopus_get_length_in_pcm_frames(decoder,
                                                            &totalFrames);
                        ma_uint64 seek_percent = get_seek_percentage();
                        if (seek_percent >= 100.0)
                                seek_percent = 100.0;
                        ma_uint64 targetFrame =
                            (ma_uint64)((totalFrames - 1) * seek_percent /
                                        100.0);

                        if (targetFrame >= totalFrames)
                                targetFrame = totalFrames - 1;

                        // Set the read pointer for the decoder
                        ma_result seekResult =
                            ma_libopus_seek_to_pcm_frame(decoder, targetFrame);
                        if (seekResult != MA_SUCCESS)
                        {
                                // Handle seek error
                                set_seek_requested(false);
                                pthread_mutex_unlock(&data_source_mutex);
                                return;
                        }

                        set_seek_requested(false); // Reset seek flag
                }

                // Read from the current decoder
                ma_uint64 framesToRead = 0;
                ma_result result;
                ma_uint64 remainingFrames = frameCount - framesRead;
                ma_libopus *first_decoder = get_first_opus_decoder();
                ma_uint64 cursor = 0;

                if (first_decoder == NULL)
                {
                        pthread_mutex_unlock(&data_source_mutex);
                        return;
                }

                if (is_EOF_reached())
                {
                        pthread_mutex_unlock(&data_source_mutex);
                        return;
                }

                result = call_read_PCM_frames(
                    first_decoder, opus->format, pFramesOut, framesRead,
                    pAudioData->channels, remainingFrames, &framesToRead);

                ma_data_source_get_cursor_in_pcm_frames(decoder, &cursor);

                if (((cursor != 0 && cursor >= pAudioData->totalFrames) ||
                     framesToRead == 0 || is_skip_to_next() ||
                     result != MA_SUCCESS) &&
                    !is_EOF_reached())
                {
                        activate_switch(pAudioData);
                        pthread_mutex_unlock(&data_source_mutex);
                        continue;
                }

                framesRead += framesToRead;
                set_buffer_size(framesToRead);

                pthread_mutex_unlock(&data_source_mutex);
        }

        set_audio_buffer(pFramesOut, framesRead, pAudioData->sample_rate,
                       pAudioData->channels, pAudioData->format);

        if (pFramesRead != NULL)
        {
                *pFramesRead = framesRead;
        }
}

void opus_on_audio_frames(ma_device *pDevice, void *pFramesOut,
                          const void *pFramesIn, ma_uint32 frameCount)
{
        AudioData *pDataSource = (AudioData *)pDevice->pUserData;
        ma_uint64 framesRead = 0;
        opus_read_pcm_frames(&(pDataSource->base), pFramesOut, frameCount,
                             &framesRead);
        (void)pFramesIn;
}

void vorbis_read_pcm_frames(ma_data_source *pDataSource, void *pFramesOut,
                            ma_uint64 frameCount, ma_uint64 *pFramesRead)
{
        ma_libvorbis *vorbis = (ma_libvorbis *)pDataSource;
        AudioData *pAudioData = (AudioData *)vorbis->pReadSeekTellUserData;

        ma_uint64 framesRead = 0;

        while (framesRead < frameCount)
        {
                if (is_impl_switch_reached())
                        return;

                if (pthread_mutex_trylock(&data_source_mutex) != 0)
                {
                        return;
                }

                // Check if a file switch is required
                if (pAudioData->switchFiles)
                {
                        execute_switch(pAudioData);
                        pthread_mutex_unlock(&data_source_mutex);
                        break;
                }

                ma_libvorbis *decoder = get_current_vorbis_decoder();

                if (pAudioData->totalFrames == 0)
                        ma_data_source_get_length_in_pcm_frames(
                            decoder, &(pAudioData->totalFrames));

                if ((get_current_implementation_type() != VORBIS &&
                     !is_skip_to_next()) ||
                    (decoder == NULL))
                {
                        pthread_mutex_unlock(&data_source_mutex);
                        return;
                }

                // Check if seeking is requested
                if (is_seek_requested())
                {
                        ma_uint64 totalFrames = 0;
                        ma_libvorbis_get_length_in_pcm_frames(decoder,
                                                              &totalFrames);
                        ma_uint64 seek_percent = get_seek_percentage();
                        if (seek_percent >= 100.0)
                                seek_percent = 100.0;
                        ma_uint64 targetFrame =
                            (ma_uint64)((totalFrames - 1) * seek_percent /
                                        100.0);

                        if (targetFrame >= totalFrames)
                                targetFrame = totalFrames - 1;

                        // Set the read pointer for the decoder
                        ma_result seekResult = ma_libvorbis_seek_to_pcm_frame(
                            decoder, targetFrame);
                        if (seekResult != MA_SUCCESS)
                        {
                                // Handle seek error
                                set_seek_requested(false);
                                pthread_mutex_unlock(&data_source_mutex);
                                return;
                        }

                        set_seek_requested(false); // Reset seek flag
                }

                // Read from the current decoder
                ma_uint64 framesToRead = 0;
                ma_result result;
                ma_uint64 framesRequested = frameCount - framesRead;
                ma_libvorbis *first_decoder = get_first_vorbis_decoder();
                ma_uint64 cursor = 0;

                if (first_decoder == NULL)
                {
                        pthread_mutex_unlock(&data_source_mutex);
                        return;
                }

                if (is_EOF_reached())
                {
                        pthread_mutex_unlock(&data_source_mutex);
                        return;
                }

                result = call_read_PCM_frames(
                    first_decoder, vorbis->format, pFramesOut, framesRead,
                    pAudioData->channels, framesRequested, &framesToRead);

                ma_data_source_get_cursor_in_pcm_frames(decoder, &cursor);

                if (((cursor != 0 && cursor >= pAudioData->totalFrames) ||
                     is_skip_to_next() || result != MA_SUCCESS) &&
                    !is_EOF_reached())
                {
                        activate_switch(pAudioData);
                        pthread_mutex_unlock(&data_source_mutex);
                        continue;
                }

                framesRead += framesToRead;
                set_buffer_size(framesToRead);

                pthread_mutex_unlock(&data_source_mutex);
        }

        set_audio_buffer(pFramesOut, framesRead, pAudioData->sample_rate,
                       pAudioData->channels, pAudioData->format);

        if (pFramesRead != NULL)
        {
                *pFramesRead = framesRead;
        }
}

void vorbis_on_audio_frames(ma_device *pDevice, void *pFramesOut,
                            const void *pFramesIn, ma_uint32 frameCount)
{
        AudioData *pDataSource = (AudioData *)pDevice->pUserData;
        ma_uint64 framesRead = 0;
        vorbis_read_pcm_frames(&(pDataSource->base), pFramesOut, frameCount,
                               &framesRead);
        (void)pFramesIn;
}

void webm_read_pcm_frames(ma_data_source *pDataSource, void *pFramesOut,
                          ma_uint64 frameCount, ma_uint64 *pFramesRead)
{
        ma_webm *webm = (ma_webm *)pDataSource;
        AudioData *pAudioData = (AudioData *)webm->pReadSeekTellUserData;

        ma_uint64 framesRead = 0;

        while (framesRead < frameCount)
        {
                if (is_impl_switch_reached())
                        return;

                if (pthread_mutex_trylock(&data_source_mutex) != 0)
                {
                        return;
                }

                // Check if a file switch is required
                if (pAudioData->switchFiles)
                {
                        execute_switch(pAudioData);
                        pthread_mutex_unlock(&data_source_mutex);
                        break;
                }

                ma_webm *decoder = get_current_webm_decoder();

                if (pAudioData->totalFrames == 0)
                        ma_data_source_get_length_in_pcm_frames(
                            decoder, &(pAudioData->totalFrames));

                if ((get_current_implementation_type() != WEBM &&
                     !is_skip_to_next()) ||
                    (decoder == NULL))
                {
                        pthread_mutex_unlock(&data_source_mutex);
                        return;
                }

                // Check if seeking is requested
                if (is_seek_requested())
                {
                        ma_uint64 totalFrames = 0;
                        ma_webm_get_length_in_pcm_frames(decoder, &totalFrames);
                        ma_uint64 seek_percent = get_seek_percentage();
                        if (seek_percent >= 100.0)
                                seek_percent = 100.0;
                        ma_uint64 targetFrame =
                            (ma_uint64)((totalFrames - 1) * seek_percent /
                                        100.0);

                        if (targetFrame >= totalFrames)
                                targetFrame = totalFrames - 1;

                        // Set the read pointer for the decoder
                        ma_result seekResult =
                            ma_webm_seek_to_pcm_frame(decoder, targetFrame);
                        if (seekResult != MA_SUCCESS)
                        {
                                // Handle seek error
                                set_seek_requested(false);
                                pthread_mutex_unlock(&data_source_mutex);
                                return;
                        }

                        set_seek_requested(false); // Reset seek flag
                }

                // Read from the current decoder
                ma_uint64 framesToRead = 0;
                ma_result result;
                ma_uint64 framesRequested = frameCount - framesRead;
                ma_webm *first_decoder = get_first_webm_decoder();
                ma_uint64 cursor = 0;

                if (first_decoder == NULL)
                {
                        pthread_mutex_unlock(&data_source_mutex);
                        return;
                }

                if (is_EOF_reached())
                {
                        pthread_mutex_unlock(&data_source_mutex);
                        return;
                }

                result = call_read_PCM_frames(
                    first_decoder, webm->format, pFramesOut, framesRead,
                    pAudioData->channels, framesRequested, &framesToRead);

                ma_data_source_get_cursor_in_pcm_frames(decoder, &cursor);

                if (((cursor != 0 && cursor >= pAudioData->totalFrames) ||
                     is_skip_to_next() || result != MA_SUCCESS) &&
                    !is_EOF_reached())
                {
                        activate_switch(pAudioData);
                        pthread_mutex_unlock(&data_source_mutex);
                        continue;
                }

                framesRead += framesToRead;
                set_buffer_size(framesToRead);

                pthread_mutex_unlock(&data_source_mutex);
        }

        set_audio_buffer(pFramesOut, framesRead, pAudioData->sample_rate,
                       pAudioData->channels, pAudioData->format);

        if (pFramesRead != NULL)
        {
                *pFramesRead = framesRead;
        }
}

void webm_on_audio_frames(ma_device *pDevice, void *pFramesOut,
                          const void *pFramesIn, ma_uint32 frameCount)
{
        AudioData *pDataSource = (AudioData *)pDevice->pUserData;
        ma_uint64 framesRead = 0;
        webm_read_pcm_frames(&(pDataSource->base), pFramesOut, frameCount,
                             &framesRead);

        if (framesRead < frameCount)
        {
                ma_webm *webm = (ma_webm *)&(pDataSource->base);
                float *output = (float *)pFramesOut;
                memset(output + framesRead * webm->channels, 0,
                       (frameCount - framesRead) * webm->channels *
                           sizeof(float));
        }
        (void)pFramesIn;
}

