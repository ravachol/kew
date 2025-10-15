
#include "common/common.h"

#include "decoders.h"
#include "audiobuffer.h"
#include "sound.h"
#include "m4a.h"

#include <stdatomic.h>

static ma_device device = {0};
static bool deviceInitialized = false;

static bool paused = false;
static bool stopped = true;
static bool repeatEnabled = false;

static bool seekRequested = false;
static float seekPercent = 0.0;
static double seekElapsed;

static bool skipToNext = false;

static _Atomic bool EOFReached = false;
static _Atomic bool switchReached = false;

static pthread_mutex_t dataSourceMutex = PTHREAD_MUTEX_INITIALIZER;

static enum AudioImplementation currentImplementation = NONE;

static double seekElapsed;

ma_uint64 lastCursor = 0;

static pthread_mutex_t switchMutex = PTHREAD_MUTEX_INITIALIZER;

double getSeekElapsed(void) { return seekElapsed; }

void setSeekElapsed(double value) { seekElapsed = value; }

float getSeekPercentage(void) { return seekPercent; }

bool isSeekRequested(void) { return seekRequested; }

void setSeekRequested(bool value) { seekRequested = value; }

void seekPercentage(float percent)
{
        seekPercent = percent;
        seekRequested = true;
}

bool isEOFReached(void) { return atomic_load(&EOFReached); }

void setEofReached(void) { atomic_store(&EOFReached, true); }

void setEofHandled(void) { atomic_store(&EOFReached, false); }

bool isPaused(void) { return paused; }

void setPaused(bool val) { paused = val; }

bool isStopped(void) { return stopped; }

void setStopped(bool val) { stopped = val; }

bool isRepeatEnabled(void) { return repeatEnabled; }

void setRepeatEnabled(bool value) { repeatEnabled = value; }

bool isPlaying(void) { return ma_device_is_started(&device); }

bool isPlaybackDone(void)
{
        if (isEOFReached())
        {
                return true;
        }
        else
        {
                return false;
        }
}

void stopPlayback(void)
{
        AppState *state = getAppState();

        if (ma_device_is_started(&device))
        {
                ma_device_stop(&device);
        }

        setPaused(false);
        setStopped(true);

        if (state->currentView != TRACK_VIEW)
        {
                triggerRefresh();
        }
}


void soundResumePlayback(void)
{
        // If this was unpaused with no song loaded

        AudioData *audioData = getAudioData();
        AppState *state = getAppState();

        if (audioData->restart)
        {
                audioData->endOfListReached = false;
        }

        if (!ma_device_is_started(&device))
        {
                if (ma_device_start(&device) != MA_SUCCESS)
                {
                        createAudioDevice();
                        ma_device_start(&device);
                }
        }

        setPaused(false);

        setStopped(false);

        if (state->currentView != TRACK_VIEW)
        {
                triggerRefresh();
        }
}

void pausePlayback(void)
{
        AppState *state = getAppState();

        if (ma_device_is_started(&device))
        {
                ma_device_stop(&device);
        }

        setPaused(true);

        if (state->currentView != TRACK_VIEW)
        {
                triggerRefresh();
        }
}

void togglePausePlayback(void)
{
        if (ma_device_is_started(&device))
        {
                pausePlayback();
        }
        else if (isPaused() || isStopped())
        {
                soundResumePlayback();
        }
}

int initPlaybackDevice(ma_context *context, ma_format format, ma_uint32 channels, ma_uint32 sampleRate,
                       ma_device *device, ma_device_data_proc dataCallback, void *pUserData)
{
        ma_result result;

        ma_device_config deviceConfig =
            ma_device_config_init(ma_device_type_playback);

        deviceConfig.playback.format = format;
        deviceConfig.playback.channels = channels;
        deviceConfig.sampleRate = sampleRate;
        deviceConfig.dataCallback = dataCallback;
        deviceConfig.pUserData = pUserData;

        result = ma_device_init(context, &deviceConfig, device);
        if (result != MA_SUCCESS)
        {
                setErrorMessage("Failed to initialize miniaudio device.");
                return -1;
        }
        else
        {
                deviceInitialized = true;
        }

        result = ma_device_start(device);

        if (result != MA_SUCCESS)
        {
                setErrorMessage("Failed to start miniaudio device.");
                return -1;
        }

        setPaused(false);

        setStopped(false);

        return 0;
}

void cleanupPlaybackDevice(void)
{
        if (!deviceInitialized)
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

        deviceInitialized = false;

        setStopped(true);
}

ma_device *getDevice(void) { return &device; }

enum AudioImplementation getCurrentImplementationType(void)
{
        return currentImplementation;
}

void getCurrentFormatAndSampleRate(ma_format *format, ma_uint32 *sampleRate)
{
        *format = ma_format_unknown;

        if (getCurrentImplementationType() == BUILTIN)
        {
                ma_decoder *decoder = getCurrentBuiltinDecoder();

                if (decoder != NULL)
                        *format = decoder->outputFormat;
        }
        else if (getCurrentImplementationType() == OPUS)
        {
                ma_libopus *decoder = getCurrentOpusDecoder();

                if (decoder != NULL)
                        *format = decoder->format;
        }
        else if (getCurrentImplementationType() == VORBIS)
        {
                ma_libvorbis *decoder = getCurrentVorbisDecoder();

                if (decoder != NULL)
                        *format = decoder->format;
        }
        else if (getCurrentImplementationType() == WEBM)
        {
                ma_webm *decoder = getCurrentWebmDecoder();

                if (decoder != NULL)
                        *format = decoder->format;
        }
        else if (getCurrentImplementationType() == M4A)
        {
#ifdef USE_FAAD
                m4a_decoder *decoder = getCurrentM4aDecoder();

                if (decoder != NULL)
                        *format = decoder->format;
#endif
        }

        *sampleRate = getAudioData()->sampleRate;
}

void executeSwitch(AudioData *pAudioData)
{
        pAudioData->switchFiles = false;
        switchDecoder();

        if (pAudioData == NULL)
                return;

        pAudioData->pUserData->currentSongData =
            (pAudioData->currentFileIndex == 0)
                ? pAudioData->pUserData->songdataA
                : pAudioData->pUserData->songdataB;
        pAudioData->totalFrames = 0;
        pAudioData->currentPCMFrame = 0;

        setSeekElapsed(0.0);

        setEofReached();
}

bool isImplSwitchReached(void)
{
        return atomic_load(&switchReached) ? true : false;
}

void setImplSwitchReached(void) { atomic_store(&switchReached, true); }

void setImplSwitchNotReached(void) { atomic_store(&switchReached, false); }

void setCurrentImplementationType(enum AudioImplementation value)
{
        currentImplementation = value;
}

bool isSkipToNext(void) { return skipToNext; }

void setSkipToNext(bool value) { skipToNext = value; }

void activateSwitch(AudioData *pAudioData)
{
        setSkipToNext(false);

        if (!isRepeatEnabled())
        {
                pthread_mutex_lock(&switchMutex);
                pAudioData->currentFileIndex =
                    1 - pAudioData->currentFileIndex; // Toggle between 0 and 1
                pthread_mutex_unlock(&switchMutex);
        }

        pAudioData->switchFiles = true;
}

void shutdownAndroid(void)
{
        // Avoid race condition when shutting down
        memset(&device, 0, sizeof(device));
}

void clearCurrentTrack(void)
{
        if (ma_device_is_started(&device))
        {
                // Stop the device (which stops playback)
                ma_device_stop(&device);
        }

        clearDecoderChain();
        resetAllDecoders();
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

                if (getCurrentImplementationType() != M4A && !isSkipToNext())
                {
                        pthread_mutex_unlock(&dataSourceMutex);
                        return;
                }

                m4a_decoder *decoder = getCurrentM4aDecoder();

                if (pAudioData->totalFrames == 0)
                        ma_data_source_get_length_in_pcm_frames(
                            decoder, &(pAudioData->totalFrames));

                // Check if seeking is requested
                if (isSeekRequested())
                {
                        if (decoder->fileType != k_rawAAC)
                        {
                                ma_uint64 totalFrames = pAudioData->totalFrames;
                                ma_uint64 seekPercent = getSeekPercentage();

                                if (seekPercent >= 100.0)
                                        seekPercent = 100.0;

                                ma_uint64 targetFrame =
                                    (ma_uint64)((totalFrames - 1) *
                                                seekPercent / 100.0);

                                if (targetFrame >= totalFrames)
                                        targetFrame = totalFrames - 1;

                                // Set the read pointer for the decoder
                                ma_result seekResult =
                                    m4a_decoder_seek_to_pcm_frame(decoder,
                                                                  targetFrame);
                                if (seekResult != MA_SUCCESS)
                                {
                                        // Handle seek error
                                        setSeekRequested(false);
                                        pthread_mutex_unlock(&dataSourceMutex);
                                        return;
                                }
                        }

                        setSeekRequested(false); // Reset seek flag
                }

                // Read from the current decoder
                ma_uint64 framesToRead = 0;
                ma_result result;
                ma_uint64 remainingFrames = frameCount - framesRead;
                m4a_decoder *firstDecoder = getFirstM4aDecoder();
                ma_uint64 cursor = 0;

                if (firstDecoder == NULL)
                {
                        pthread_mutex_unlock(&dataSourceMutex);
                        return;
                }

                if (isEOFReached())
                {
                        pthread_mutex_unlock(&dataSourceMutex);
                        return;
                }

                result = callReadPCMFrames(
                    firstDecoder, m4a->format, pFramesOut, framesRead,
                    pAudioData->channels, remainingFrames, &framesToRead);

                ma_data_source_get_cursor_in_pcm_frames(decoder, &cursor);

                if (((cursor != 0 && cursor == lastCursor) ||
                     framesToRead == 0 || isSkipToNext() ||
                     result != MA_SUCCESS) &&
                    !isEOFReached())
                {
                        activateSwitch(pAudioData);
                        pthread_mutex_unlock(&dataSourceMutex);
                        continue;
                }

                lastCursor = cursor;

                framesRead += framesToRead;
                setBufferSize(framesToRead);

                pthread_mutex_unlock(&dataSourceMutex);
        }

        setAudioBuffer(pFramesOut, framesRead, pAudioData->sampleRate,
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

                if (getCurrentImplementationType() != OPUS && !isSkipToNext())
                {
                        pthread_mutex_unlock(&dataSourceMutex);
                        return;
                }

                ma_libopus *decoder = getCurrentOpusDecoder();

                if (pAudioData->totalFrames == 0)
                        ma_data_source_get_length_in_pcm_frames(
                            decoder, &(pAudioData->totalFrames));

                // Check if seeking is requested
                if (isSeekRequested())
                {
                        ma_uint64 totalFrames = 0;
                        ma_libopus_get_length_in_pcm_frames(decoder,
                                                            &totalFrames);
                        ma_uint64 seekPercent = getSeekPercentage();
                        if (seekPercent >= 100.0)
                                seekPercent = 100.0;
                        ma_uint64 targetFrame =
                            (ma_uint64)((totalFrames - 1) * seekPercent /
                                        100.0);

                        if (targetFrame >= totalFrames)
                                targetFrame = totalFrames - 1;

                        // Set the read pointer for the decoder
                        ma_result seekResult =
                            ma_libopus_seek_to_pcm_frame(decoder, targetFrame);
                        if (seekResult != MA_SUCCESS)
                        {
                                // Handle seek error
                                setSeekRequested(false);
                                pthread_mutex_unlock(&dataSourceMutex);
                                return;
                        }

                        setSeekRequested(false); // Reset seek flag
                }

                // Read from the current decoder
                ma_uint64 framesToRead = 0;
                ma_result result;
                ma_uint64 remainingFrames = frameCount - framesRead;
                ma_libopus *firstDecoder = getFirstOpusDecoder();
                ma_uint64 cursor = 0;

                if (firstDecoder == NULL)
                {
                        pthread_mutex_unlock(&dataSourceMutex);
                        return;
                }

                if (isEOFReached())
                {
                        pthread_mutex_unlock(&dataSourceMutex);
                        return;
                }

                result = callReadPCMFrames(
                    firstDecoder, opus->format, pFramesOut, framesRead,
                    pAudioData->channels, remainingFrames, &framesToRead);

                ma_data_source_get_cursor_in_pcm_frames(decoder, &cursor);

                if (((cursor != 0 && cursor >= pAudioData->totalFrames) ||
                     framesToRead == 0 || isSkipToNext() ||
                     result != MA_SUCCESS) &&
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

        setAudioBuffer(pFramesOut, framesRead, pAudioData->sampleRate,
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
                        break;
                }

                ma_libvorbis *decoder = getCurrentVorbisDecoder();

                if (pAudioData->totalFrames == 0)
                        ma_data_source_get_length_in_pcm_frames(
                            decoder, &(pAudioData->totalFrames));

                if ((getCurrentImplementationType() != VORBIS &&
                     !isSkipToNext()) ||
                    (decoder == NULL))
                {
                        pthread_mutex_unlock(&dataSourceMutex);
                        return;
                }

                // Check if seeking is requested
                if (isSeekRequested())
                {
                        ma_uint64 totalFrames = 0;
                        ma_libvorbis_get_length_in_pcm_frames(decoder,
                                                              &totalFrames);
                        ma_uint64 seekPercent = getSeekPercentage();
                        if (seekPercent >= 100.0)
                                seekPercent = 100.0;
                        ma_uint64 targetFrame =
                            (ma_uint64)((totalFrames - 1) * seekPercent /
                                        100.0);

                        if (targetFrame >= totalFrames)
                                targetFrame = totalFrames - 1;

                        // Set the read pointer for the decoder
                        ma_result seekResult = ma_libvorbis_seek_to_pcm_frame(
                            decoder, targetFrame);
                        if (seekResult != MA_SUCCESS)
                        {
                                // Handle seek error
                                setSeekRequested(false);
                                pthread_mutex_unlock(&dataSourceMutex);
                                return;
                        }

                        setSeekRequested(false); // Reset seek flag
                }

                // Read from the current decoder
                ma_uint64 framesToRead = 0;
                ma_result result;
                ma_uint64 framesRequested = frameCount - framesRead;
                ma_libvorbis *firstDecoder = getFirstVorbisDecoder();
                ma_uint64 cursor = 0;

                if (firstDecoder == NULL)
                {
                        pthread_mutex_unlock(&dataSourceMutex);
                        return;
                }

                if (isEOFReached())
                {
                        pthread_mutex_unlock(&dataSourceMutex);
                        return;
                }

                result = callReadPCMFrames(
                    firstDecoder, vorbis->format, pFramesOut, framesRead,
                    pAudioData->channels, framesRequested, &framesToRead);

                ma_data_source_get_cursor_in_pcm_frames(decoder, &cursor);

                if (((cursor != 0 && cursor >= pAudioData->totalFrames) ||
                     isSkipToNext() || result != MA_SUCCESS) &&
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

        setAudioBuffer(pFramesOut, framesRead, pAudioData->sampleRate,
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
                        break;
                }

                ma_webm *decoder = getCurrentWebmDecoder();

                if (pAudioData->totalFrames == 0)
                        ma_data_source_get_length_in_pcm_frames(
                            decoder, &(pAudioData->totalFrames));

                if ((getCurrentImplementationType() != WEBM &&
                     !isSkipToNext()) ||
                    (decoder == NULL))
                {
                        pthread_mutex_unlock(&dataSourceMutex);
                        return;
                }

                // Check if seeking is requested
                if (isSeekRequested())
                {
                        ma_uint64 totalFrames = 0;
                        ma_webm_get_length_in_pcm_frames(decoder, &totalFrames);
                        ma_uint64 seekPercent = getSeekPercentage();
                        if (seekPercent >= 100.0)
                                seekPercent = 100.0;
                        ma_uint64 targetFrame =
                            (ma_uint64)((totalFrames - 1) * seekPercent /
                                        100.0);

                        if (targetFrame >= totalFrames)
                                targetFrame = totalFrames - 1;

                        // Set the read pointer for the decoder
                        ma_result seekResult =
                            ma_webm_seek_to_pcm_frame(decoder, targetFrame);
                        if (seekResult != MA_SUCCESS)
                        {
                                // Handle seek error
                                setSeekRequested(false);
                                pthread_mutex_unlock(&dataSourceMutex);
                                return;
                        }

                        setSeekRequested(false); // Reset seek flag
                }

                // Read from the current decoder
                ma_uint64 framesToRead = 0;
                ma_result result;
                ma_uint64 framesRequested = frameCount - framesRead;
                ma_webm *firstDecoder = getFirstWebmDecoder();
                ma_uint64 cursor = 0;

                if (firstDecoder == NULL)
                {
                        pthread_mutex_unlock(&dataSourceMutex);
                        return;
                }

                if (isEOFReached())
                {
                        pthread_mutex_unlock(&dataSourceMutex);
                        return;
                }

                result = callReadPCMFrames(
                    firstDecoder, webm->format, pFramesOut, framesRead,
                    pAudioData->channels, framesRequested, &framesToRead);

                ma_data_source_get_cursor_in_pcm_frames(decoder, &cursor);

                if (((cursor != 0 && cursor >= pAudioData->totalFrames) ||
                     isSkipToNext() || result != MA_SUCCESS) &&
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

        setAudioBuffer(pFramesOut, framesRead, pAudioData->sampleRate,
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

