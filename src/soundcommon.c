#include "soundcommon.h"

#define MAX_DECODERS 2

/*

soundcommon.c

 Related to common functions for decoders / miniaudio implementations

*/

bool allowNotifications = true;
bool repeatEnabled = false;
bool shuffleEnabled = false;
bool skipToNext = false;
bool seekRequested = false;
bool paused = false;
float seekPercent = 0.0;
double seekElapsed;
_Atomic bool EOFReached = false;
_Atomic bool switchReached = false;
_Atomic bool readingFrames = false;
pthread_mutex_t dataSourceMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t switchMutex = PTHREAD_MUTEX_INITIALIZER;
ma_device device = {0};
ma_int32 *audioBuffer = NULL;
ma_decoder *firstDecoder;
ma_decoder *currentDecoder;
int bufSize;
ma_event switchAudioImpl;
enum AudioImplementation currentImplementation = NONE;
ma_decoder *decoders[MAX_DECODERS];
ma_libopus *opusDecoders[MAX_DECODERS];
ma_libopus *firstOpusDecoder;
ma_libvorbis *vorbisDecoders[MAX_DECODERS];
ma_libvorbis *firstVorbisDecoder;
m4a_decoder *m4aDecoders[MAX_DECODERS];
m4a_decoder *firstM4aDecoder;
int decoderIndex = -1;
int m4aDecoderIndex = -1;
int opusDecoderIndex = -1;
int vorbisDecoderIndex = -1;
bool doQuit = false;

int soundVolume = 100;

enum AudioImplementation getCurrentImplementationType()
{
        return currentImplementation;
}

void setCurrentImplementationType(enum AudioImplementation value)
{
        currentImplementation = value;
}

ma_decoder *getFirstDecoder()
{
        return firstDecoder;
}

ma_decoder *getCurrentBuiltinDecoder()
{
        if (decoderIndex == -1)
                return getFirstDecoder();
        else
                return decoders[decoderIndex];
}

void switchDecoder()
{
        if (decoderIndex == -1)
                decoderIndex = 0;
        else
                decoderIndex = 1 - decoderIndex;
}

void setNextDecoder(ma_decoder *decoder)
{
        if (decoderIndex == -1 && firstDecoder == NULL)
        {
                firstDecoder = decoder;
        }
        else if (decoderIndex == -1)
        {
                if (decoders[0] != NULL)
                {
                        ma_decoder_uninit(decoders[0]);
                        free(decoders[0]);
                        decoders[0] = NULL;
                }

                decoders[0] = decoder;
        }
        else
        {
                int nextIndex = 1 - decoderIndex;

                if (decoders[nextIndex] != NULL)
                {
                        ma_decoder_uninit(decoders[nextIndex]);
                        free(decoders[nextIndex]);
                        decoders[nextIndex] = NULL;
                }

                decoders[nextIndex] = decoder;
        }
}

void resetDecoders()
{
        decoderIndex = -1;

        if (firstDecoder != NULL && firstDecoder->outputFormat != ma_format_unknown)
        {
                ma_decoder_uninit(firstDecoder);
                free(firstDecoder);
                firstDecoder = NULL;
        }

        if (decoders[0] != NULL && decoders[0]->outputFormat != ma_format_unknown)
        {
                ma_decoder_uninit(decoders[0]);
                free(decoders[0]);
                decoders[0] = NULL;
        }

        if (decoders[1] != NULL && decoders[1]->outputFormat != ma_format_unknown)
        {
                ma_decoder_uninit(decoders[1]);
                free(decoders[1]);
                decoders[1] = NULL;
        }
}

void uninitPreviousDecoder()
{
        if (decoderIndex == -1)
        {
                return;
        }
        ma_decoder *toUninit = decoders[1 - decoderIndex];

        if (toUninit != NULL)
        {
                ma_decoder_uninit(toUninit);
                free(toUninit);
                decoders[1 - decoderIndex] = NULL;
        }
}

void uninitPreviousVorbisDecoder()
{
        if (vorbisDecoderIndex == -1)
        {
                return;
        }
        ma_libvorbis *toUninit = vorbisDecoders[1 - vorbisDecoderIndex];

        if (toUninit != NULL)
        {
                ma_libvorbis_uninit(toUninit, NULL);
                free(toUninit);
                vorbisDecoders[1 - vorbisDecoderIndex] = NULL;
        }
}

void uninitPreviousM4aDecoder()
{
        if (m4aDecoderIndex == -1) // either start of the program or resetM4aDecoders has been called
        {
                return;
        }
        m4a_decoder *toUninit = m4aDecoders[1 - m4aDecoderIndex];

        if (toUninit != NULL)
        {
                m4a_decoder_uninit(toUninit, NULL);
                free(toUninit);
                m4aDecoders[1 - m4aDecoderIndex] = NULL;
        }
}

void uninitPreviousOpusDecoder()
{
        if (opusDecoderIndex == -1)
        {
                return;
        }
        ma_libopus *toUninit = opusDecoders[1 - opusDecoderIndex];

        if (toUninit != NULL)
        {
                ma_libopus_uninit(toUninit, NULL);
                free(toUninit);
                opusDecoders[1 - opusDecoderIndex] = NULL;
        }
}

ma_libvorbis *getFirstVorbisDecoder()
{
        return firstVorbisDecoder;
}

m4a_decoder *getFirstM4aDecoder()
{
        return firstM4aDecoder;
}

ma_libopus *getFirstOpusDecoder()
{
        return firstOpusDecoder;
}

ma_libvorbis *getCurrentVorbisDecoder()
{
        if (vorbisDecoderIndex == -1)
                return getFirstVorbisDecoder();
        else
                return vorbisDecoders[vorbisDecoderIndex];
}

m4a_decoder *getCurrentM4aDecoder()
{
        if (m4aDecoderIndex == -1)
                return getFirstM4aDecoder();
        else
                return m4aDecoders[m4aDecoderIndex];
}

ma_libopus *getCurrentOpusDecoder()
{
        if (opusDecoderIndex == -1)
                return getFirstOpusDecoder();
        else
                return opusDecoders[opusDecoderIndex];
}

ma_format getCurrentFormat()
{
        ma_format format = ma_format_unknown;

        if (getCurrentImplementationType() == BUILTIN)
        {
                ma_decoder *decoder = getCurrentBuiltinDecoder();

                if (decoder != NULL)
                        format = decoder->outputFormat;
        }
        else if (getCurrentImplementationType() == OPUS)
        {
                ma_libopus *decoder = getCurrentOpusDecoder();

                if (decoder != NULL)
                        format = decoder->format;
        }
        else if (getCurrentImplementationType() == VORBIS)
        {
                ma_libvorbis *decoder = getCurrentVorbisDecoder();

                if (decoder != NULL)
                        format = decoder->format;
        }
        else if (getCurrentImplementationType() == M4A)
        {
                m4a_decoder *decoder = getCurrentM4aDecoder();

                if (decoder != NULL)
                        format = decoder->format;
        }

        return format;
}

void switchVorbisDecoder()
{
        if (vorbisDecoderIndex == -1)
                vorbisDecoderIndex = 0;
        else
                vorbisDecoderIndex = 1 - vorbisDecoderIndex;
}

void switchM4aDecoder()
{
        if (m4aDecoderIndex == -1)
                m4aDecoderIndex = 0;
        else
                m4aDecoderIndex = 1 - m4aDecoderIndex;
}

void switchOpusDecoder()
{
        if (opusDecoderIndex == -1)
                opusDecoderIndex = 0;
        else
                opusDecoderIndex = 1 - opusDecoderIndex;
}

void setNextVorbisDecoder(ma_libvorbis *decoder)
{
        if (vorbisDecoderIndex == -1 && firstVorbisDecoder == NULL)
        {
                firstVorbisDecoder = decoder;
        }
        else if (vorbisDecoderIndex == -1)
        {
                if (vorbisDecoders[0] != NULL)
                {
                        ma_libvorbis_uninit(vorbisDecoders[0], NULL);
                        free(vorbisDecoders[0]);
                        vorbisDecoders[0] = NULL;
                }

                vorbisDecoders[0] = decoder;
        }
        else
        {
                int nextIndex = 1 - vorbisDecoderIndex;

                if (vorbisDecoders[nextIndex] != NULL)
                {
                        ma_libvorbis_uninit(vorbisDecoders[nextIndex], NULL);
                        free(vorbisDecoders[nextIndex]);
                        vorbisDecoders[nextIndex] = NULL;
                }

                vorbisDecoders[nextIndex] = decoder;
        }
}

void setNextM4aDecoder(m4a_decoder *decoder)
{
        if (m4aDecoderIndex == -1 && firstM4aDecoder == NULL)
        {
                firstM4aDecoder = decoder;
        }
        else if (m4aDecoderIndex == -1) // array hasn't been used yet
        {
                if (m4aDecoders[0] != NULL)
                {
                        m4a_decoder_uninit(m4aDecoders[0], NULL);
                        free(m4aDecoders[0]);
                        m4aDecoders[0] = NULL;
                }

                m4aDecoders[0] = decoder;
        }
        else
        {
                int nextIndex = 1 - m4aDecoderIndex;
                if (m4aDecoders[nextIndex] != NULL)
                {
                        m4a_decoder_uninit(m4aDecoders[nextIndex], NULL);
                        free(m4aDecoders[nextIndex]);
                        m4aDecoders[nextIndex] = NULL;
                }

                m4aDecoders[nextIndex] = decoder;
        }
}

void setNextOpusDecoder(ma_libopus *decoder)
{
        if (opusDecoderIndex == -1 && firstOpusDecoder == NULL)
        {
                firstOpusDecoder = decoder;
        }
        else if (opusDecoderIndex == -1)
        {
                if (opusDecoders[0] != NULL)
                {
                        ma_libopus_uninit(opusDecoders[0], NULL);
                        free(opusDecoders[0]);
                        opusDecoders[0] = NULL;
                }
                opusDecoders[0] = decoder;
        }
        else
        {
                int nextIndex = 1 - opusDecoderIndex;
                if (opusDecoders[nextIndex] != NULL)
                {
                        ma_libopus_uninit(opusDecoders[nextIndex], NULL);
                        free(opusDecoders[nextIndex]);
                        opusDecoders[nextIndex] = NULL;
                }
                opusDecoders[nextIndex] = decoder;
        }
}

void resetVorbisDecoders()
{
        vorbisDecoderIndex = -1;

        if (firstVorbisDecoder != NULL && firstVorbisDecoder->format != ma_format_unknown)
        {
                ma_libvorbis_uninit(firstVorbisDecoder, NULL);
                free(firstVorbisDecoder);
                firstVorbisDecoder = NULL;
        }

        if (vorbisDecoders[0] != NULL && vorbisDecoders[0]->format != ma_format_unknown)
        {
                ma_libvorbis_uninit(vorbisDecoders[0], NULL);
                free(vorbisDecoders[0]);
                vorbisDecoders[0] = NULL;
        }

        if (vorbisDecoders[1] != NULL && vorbisDecoders[1]->format != ma_format_unknown)
        {
                ma_libvorbis_uninit(vorbisDecoders[1], NULL);
                free(vorbisDecoders[1]);
                vorbisDecoders[1] = NULL;
        }
}

void resetM4aDecoders()
{
        m4aDecoderIndex = -1;

        if (firstM4aDecoder != NULL && firstM4aDecoder->format != ma_format_unknown)
        {
                m4a_decoder_uninit(firstM4aDecoder, NULL);
                free(firstM4aDecoder);
                firstM4aDecoder = NULL;
        }

        if (m4aDecoders[0] != NULL && m4aDecoders[0]->format != ma_format_unknown)
        {
                m4a_decoder_uninit(m4aDecoders[0], NULL);
                free(m4aDecoders[0]);
                m4aDecoders[0] = NULL;
        }

        if (m4aDecoders[1] != NULL && m4aDecoders[1]->format != ma_format_unknown)
        {
                m4a_decoder_uninit(m4aDecoders[1], NULL);
                free(m4aDecoders[1]);
                m4aDecoders[1] = NULL;
        }
}

void resetOpusDecoders()
{
        opusDecoderIndex = -1;

        if (firstOpusDecoder != NULL && firstOpusDecoder->format != ma_format_unknown)
        {
                ma_libopus_uninit(firstOpusDecoder, NULL);
                free(firstOpusDecoder);
                firstOpusDecoder = NULL;
        }

        if (opusDecoders[0] != NULL && opusDecoders[0]->format != ma_format_unknown)
        {
                ma_libopus_uninit(opusDecoders[0], NULL);
                ma_free(opusDecoders[0], NULL);
                opusDecoders[0] = NULL;
        }

        if (opusDecoders[1] != NULL && opusDecoders[1]->format != ma_format_unknown)
        {
                ma_libopus_uninit(opusDecoders[1], NULL);
                ma_free(opusDecoders[1], NULL);
                opusDecoders[1] = NULL;
        }
}

int prepareNextVorbisDecoder(char *filepath)
{
        ma_libvorbis *currentDecoder;

        if (vorbisDecoderIndex == -1)
        {
                currentDecoder = getFirstVorbisDecoder();
        }
        else
        {
                currentDecoder = vorbisDecoders[vorbisDecoderIndex];
        }

        ma_uint32 sampleRate;
        ma_uint32 channels;
        ma_format format;
        ma_channel channelMap[MA_MAX_CHANNELS];
        ma_libvorbis_get_data_format(currentDecoder, &format, &channels, &sampleRate, channelMap, MA_MAX_CHANNELS);

        uninitPreviousVorbisDecoder();

        ma_libvorbis *decoder = (ma_libvorbis *)malloc(sizeof(ma_libvorbis));
        ma_result result = ma_libvorbis_init_file(filepath, NULL, NULL, decoder);

        if (result != MA_SUCCESS)
                return -1;

        ma_format nformat;
        ma_uint32 nchannels;
        ma_uint32 nsampleRate;
        ma_channel nchannelMap[MA_MAX_CHANNELS];

        ma_libvorbis_get_data_format(decoder, &nformat, &nchannels, &nsampleRate, nchannelMap, MA_MAX_CHANNELS);
        bool sameFormat = (currentDecoder == NULL || (format == nformat &&
                                                      channels == nchannels &&
                                                      sampleRate == nsampleRate));

        if (!sameFormat)
        {
                ma_libvorbis_uninit(decoder, NULL);
                free(decoder);
                return -1;
        }

        ma_libvorbis *first = getFirstVorbisDecoder();
        if (first != NULL)
        {
                decoder->pReadSeekTellUserData = (AudioData *)first->pReadSeekTellUserData;
        }

        decoder->format = nformat;
        decoder->onRead = ma_libvorbis_read_pcm_frames_wrapper;
        decoder->onSeek = ma_libvorbis_seek_to_pcm_frame_wrapper;
        decoder->onTell = ma_libvorbis_get_cursor_in_pcm_frames_wrapper;

        setNextVorbisDecoder(decoder);
        if (currentDecoder != NULL)
                ma_data_source_set_next(currentDecoder, decoder);

        return 0;
}

int prepareNextM4aDecoder(char *filepath)
{
        m4a_decoder *currentDecoder;

        if (m4aDecoderIndex == -1)
        {
                currentDecoder = getFirstM4aDecoder();
        }
        else
        {
                currentDecoder = m4aDecoders[m4aDecoderIndex];
        }

        ma_uint32 sampleRate;
        ma_uint32 channels;
        ma_format format;
        ma_channel channelMap[MA_MAX_CHANNELS];
        m4a_decoder_get_data_format(currentDecoder, &format, &channels, &sampleRate, channelMap, MA_MAX_CHANNELS);

        uninitPreviousM4aDecoder();

        m4a_decoder *decoder = (m4a_decoder *)malloc(sizeof(m4a_decoder));
        ma_result result = m4a_decoder_init_file(filepath, NULL, NULL, decoder);

        if (result != MA_SUCCESS)
                return -1;

        ma_format nformat;
        ma_uint32 nchannels;
        ma_uint32 nsampleRate;
        ma_channel nchannelMap[MA_MAX_CHANNELS];

        m4a_decoder_get_data_format(decoder, &nformat, &nchannels, &nsampleRate, nchannelMap, MA_MAX_CHANNELS);
        bool sameFormat = (currentDecoder == NULL || (format == nformat &&
                                                      channels == nchannels &&
                                                      sampleRate == nsampleRate));

        if (!sameFormat)
        {
                m4a_decoder_uninit(decoder, NULL);
                free(decoder);
                return -1;
        }

        m4a_decoder *first = getFirstM4aDecoder();
        if (first != NULL)
        {
                decoder->pReadSeekTellUserData = (AudioData *)first->pReadSeekTellUserData;
        }

        decoder->format = nformat;
        decoder->onRead = m4a_read_pcm_frames_wrapper;
        decoder->onSeek = m4a_seek_to_pcm_frame_wrapper;
        decoder->onTell = m4a_get_cursor_in_pcm_frames_wrapper;
        decoder->cursor = 0;

        setNextM4aDecoder(decoder);
        if (currentDecoder != NULL)
                ma_data_source_set_next(currentDecoder, decoder);

        return 0;
}

int prepareNextOpusDecoder(char *filepath)
{
        ma_libopus *currentDecoder;

        if (opusDecoderIndex == -1)
        {
                currentDecoder = getFirstOpusDecoder();
        }
        else
        {
                currentDecoder = opusDecoders[decoderIndex];
        }

        ma_uint32 sampleRate;
        ma_uint32 channels;
        ma_format format;
        ma_channel channelMap[MA_MAX_CHANNELS];
        ma_libopus_get_data_format(currentDecoder, &format, &channels, &sampleRate, channelMap, MA_MAX_CHANNELS);

        uninitPreviousOpusDecoder();

        ma_libopus *decoder = (ma_libopus *)malloc(sizeof(ma_libopus));
        ma_result result = ma_libopus_init_file(filepath, NULL, NULL, decoder);

        if (result != MA_SUCCESS)
                return -1;

        ma_format nformat;
        ma_uint32 nchannels;
        ma_uint32 nsampleRate;
        ma_channel nchannelMap[MA_MAX_CHANNELS];

        ma_libopus_get_data_format(decoder, &nformat, &nchannels, &nsampleRate, nchannelMap, MA_MAX_CHANNELS);
        bool sameFormat = (currentDecoder == NULL || (format == nformat &&
                                                      channels == nchannels &&
                                                      sampleRate == nsampleRate));

        if (!sameFormat)
        {
                ma_libopus_uninit(decoder, NULL);
                free(decoder);
                return -1;
        }

        ma_libopus *first = getFirstOpusDecoder();

        if (first != NULL)
        {
                decoder->pReadSeekTellUserData = (AudioData *)first->pReadSeekTellUserData;
        }

        decoder->format = nformat;
        decoder->onRead = ma_libopus_read_pcm_frames_wrapper;
        decoder->onSeek = ma_libopus_seek_to_pcm_frame_wrapper;
        decoder->onTell = ma_libopus_get_cursor_in_pcm_frames_wrapper;

        setNextOpusDecoder(decoder);

        if (currentDecoder != NULL)
                ma_data_source_set_next(currentDecoder, decoder);

        return 0;
}

void getFileInfo(const char *filename, ma_uint32 *sampleRate, ma_uint32 *channels, ma_format *format)
{
        ma_decoder tmp;
        if (ma_decoder_init_file(filename, NULL, &tmp) == MA_SUCCESS)
        {
                *sampleRate = tmp.outputSampleRate;
                *channels = tmp.outputChannels;
                *format = tmp.outputFormat;
                ma_decoder_uninit(&tmp);
        }
        else
        {
                // Handle file open error.
        }
}

int prepareNextDecoder(char *filepath)
{
        ma_decoder *currentDecoder;

        if (decoderIndex == -1)
        {
                currentDecoder = getFirstDecoder();
        }
        else
        {
                currentDecoder = decoders[decoderIndex];
        }

        ma_uint32 sampleRate;
        ma_uint32 channels;
        ma_format format;
        getFileInfo(filepath, &sampleRate, &channels, &format);

        bool sameFormat = (currentDecoder == NULL || (format == currentDecoder->outputFormat &&
                                                      channels == currentDecoder->outputChannels &&
                                                      sampleRate == currentDecoder->outputSampleRate));

        if (!sameFormat)
        {
                return 0;
        }

        uninitPreviousDecoder();

        ma_decoder *decoder = (ma_decoder *)malloc(sizeof(ma_decoder));
        ma_result result = ma_decoder_init_file(filepath, NULL, decoder);

        if (result != MA_SUCCESS)
        {
                free(decoder);
                return -1;
        }
        setNextDecoder(decoder);

        if (currentDecoder != NULL)
                ma_data_source_set_next(currentDecoder, decoder);

        return 0;
}

void getVorbisFileInfo(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sampleRate, ma_channel *channelMap)
{
        ma_libvorbis decoder;
        if (ma_libvorbis_init_file(filename, NULL, NULL, &decoder) == MA_SUCCESS)
        {
                *format = decoder.format;
                ma_libvorbis_get_data_format(&decoder, format, channels, sampleRate, channelMap, MA_MAX_CHANNELS);
                ma_libvorbis_uninit(&decoder, NULL);
        }
}

void getM4aFileInfo(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sampleRate, ma_channel *channelMap)
{
        m4a_decoder decoder;
        if (m4a_decoder_init_file(filename, NULL, NULL, &decoder) == MA_SUCCESS)
        {
                *format = decoder.format;
                m4a_decoder_get_data_format(&decoder, format, channels, sampleRate, channelMap, MA_MAX_CHANNELS);
                m4a_decoder_uninit(&decoder, NULL);
        }
}

void getOpusFileInfo(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sampleRate, ma_channel *channelMap)
{
        ma_libopus decoder;

        if (ma_libopus_init_file(filename, NULL, NULL, &decoder) == MA_SUCCESS)
        {
                *format = decoder.format;
                ma_libopus_get_data_format(&decoder, format, channels, sampleRate, channelMap, MA_MAX_CHANNELS);
                ma_libopus_uninit(&decoder, NULL);
        }
}

int getBufferSize()
{
        return bufSize;
}

void setBufferSize(int value)
{
        bufSize = value;
}

void initAudioBuffer()
{
        if (audioBuffer == NULL)
        {
                audioBuffer = malloc(sizeof(ma_int32) * MAX_BUFFER_SIZE);
                if (audioBuffer == NULL)
                {
                        // Memory allocation failed
                        return;
                }
        }
}

ma_int32 *getAudioBuffer()
{
        return audioBuffer;
}

void setAudioBuffer(ma_int32 *buf)
{
        audioBuffer = buf;
}

void resetAudioBuffer()
{
        memset(audioBuffer, 0, sizeof(float) * MAX_BUFFER_SIZE);
}

void freeAudioBuffer()
{
        if (audioBuffer != NULL)
        {
                free(audioBuffer);
                audioBuffer = NULL;
        }
}

bool isRepeatEnabled()
{
        return repeatEnabled;
}

void setRepeatEnabled(bool value)
{
        repeatEnabled = value;
}

bool isShuffleEnabled()
{
        return shuffleEnabled;
}

void setShuffleEnabled(bool value)
{
        shuffleEnabled = value;
}

bool isSkipToNext()
{
        return skipToNext;
}

void setSkipToNext(bool value)
{
        skipToNext = value;
}

double getSeekElapsed()
{
        return seekElapsed;
}

double getPercentageElapsed()
{
        return elapsedSeconds / duration;
}

void setSeekElapsed(double value)
{
        seekElapsed = value;
}

bool isEOFReached()
{
        return atomic_load(&EOFReached);
}

void setEOFReached()
{
        atomic_store(&EOFReached, true);
}

void setEOFNotReached()
{
        atomic_store(&EOFReached, false);
}

bool isImplSwitchReached()
{
        return atomic_load(&switchReached) ? true : false;
}

void setImplSwitchReached()
{
        atomic_store(&switchReached, true);
}

void setImplSwitchNotReached()
{
        atomic_store(&switchReached, false);
}

bool isPlaying()
{
        return ma_device_is_started(&device);
}

bool isPlaybackDone()
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

float getSeekPercentage()
{
        return seekPercent;
}

bool isSeekRequested()
{
        return seekRequested;
}

void setSeekRequested(bool value)
{
        seekRequested = value;
}

void seekPercentage(float percent)
{
        seekPercent = percent;
        seekRequested = true;
}

void resumePlayback()
{
        if (!ma_device_is_started(&device))
        {
                ma_device_start(&device);
        }

        paused = false;

        if (appState.currentView != SONG_VIEW)
        {
                refresh = true;
        }
}

void pausePlayback()
{
        if (ma_device_is_started(&device))
        {
                ma_device_stop(&device);
        }

        paused = true;

        if (appState.currentView != SONG_VIEW)
        {
                refresh = true;
        }
}

void cleanupPlaybackDevice()
{
        ma_device_stop(&device);
        while (ma_device_get_state(&device) != ma_device_state_stopped && ma_device_get_state(&device) != ma_device_state_uninitialized)
        {
                c_sleep(100);
        }
        ma_device_uninit(&device);
}

void togglePausePlayback()
{
        if (ma_device_is_started(&device))
        {
                pausePlayback();
        }
        else if (paused)
        {
                resumePlayback();
        }
}

bool isPaused()
{
        return paused;
}

pthread_mutex_t deviceMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t deviceStopped = PTHREAD_COND_INITIALIZER;

void resetDevice()
{
        pthread_mutex_lock(&deviceMutex);

        if (ma_device_get_state(&device) == ma_device_state_started)
                ma_device_stop(&device);

        while (ma_device_get_state(&device) == ma_device_state_started)
        {
                pthread_cond_wait(&deviceStopped, &deviceMutex);
        }

        pthread_mutex_unlock(&deviceMutex);

        ma_device_uninit(&device);
}

ma_device *getDevice()
{
        return &device;
}

bool hasBuiltinDecoder(char *filePath)
{
        char *extension = strrchr(filePath, '.');
        return (extension != NULL && (strcasecmp(extension, ".wav") == 0 || strcasecmp(extension, ".flac") == 0 ||
                                      strcasecmp(extension, ".mp3") == 0));
}

void activateSwitch(AudioData *pAudioData)
{
        setSkipToNext(false);

        if (!isRepeatEnabled())
        {
                pthread_mutex_lock(&switchMutex);
                pAudioData->currentFileIndex = 1 - pAudioData->currentFileIndex; // Toggle between 0 and 1
                pthread_mutex_unlock(&switchMutex);
        }

        pAudioData->switchFiles = true;
}

void sanitize_filepath(const char *input, char *sanitized, size_t size)
{
        size_t i, j = 0;

        if (input != NULL)
        {
                for (i = 0; i < strlen(input) && j < size - 1; ++i)
                {
                        if (isalnum((unsigned char)input[i]) || strchr(" :[]()/.,?!-", input[i]))
                        {
                                sanitized[j++] = input[i];
                        }
                }
        }

        sanitized[j] = '\0';
}

char *remove_blacklisted_chars(const char *input, const char *blacklist)
{
        if (!input || !blacklist)
                return NULL;

        char *output = malloc(strlen(input) + 1);
        if (!output)
        {
                perror("Failed to allocate memory");
                exit(EXIT_FAILURE);
        }

        const char *in_ptr = input;
        char *out_ptr = output;
        while (*in_ptr)
        {
                // If the current character is not in the blacklist, copy it to the output
                if (!strchr(blacklist, *in_ptr))
                {
                        *out_ptr++ = *in_ptr;
                }
                in_ptr++;
        }
        *out_ptr = '\0';

        return output;
}

int displaySongNotification(const char *artist, const char *title, const char *cover)
{
        if (!allowNotifications)
                return 0;

        char command[MAXPATHLEN + 1024];
        char sanitized_cover[MAXPATHLEN];

        const char *blacklist = "&;`|*~<>^()[]{}$\\\"";
        char *sanitizedArtist = remove_blacklisted_chars(artist, blacklist);
        char *sanitizedTitle = remove_blacklisted_chars(title, blacklist);

        sanitize_filepath(cover, sanitized_cover, sizeof(sanitized_cover));

        if (strlen(artist) <= 0)
        {
                snprintf(command, sizeof(command), "notify-send -a \"kew\" \"%s\" --icon \"%s\"", sanitizedTitle, sanitized_cover);
        }
        else
        {
                snprintf(command, sizeof(command), "notify-send -a \"kew\" \"%s - %s\" --icon \"%s\"", sanitizedArtist, sanitizedTitle, sanitized_cover);
        }

        free(sanitizedArtist);
        free(sanitizedTitle);

        return system(command);
}

void executeSwitch(AudioData *pAudioData)
{
        pAudioData->switchFiles = false;
        switchDecoder();
        switchOpusDecoder();
        switchM4aDecoder();
        switchVorbisDecoder();

        pAudioData->totalFrames = 0;

        SongData *currentSongData = NULL;

        if (pAudioData->currentFileIndex == 0)
        {
                if (pAudioData->pUserData->songdataA != NULL)
                {
                        currentSongData = pAudioData->pUserData->songdataA;
                }
                else
                {
                        currentSongData = NULL;
                        pAudioData->fileA = NULL;
                }
        }
        else
        {
                if (pAudioData->pUserData->songdataB != NULL)
                {
                        currentSongData = pAudioData->pUserData->songdataB;
                }
                else
                {
                        currentSongData = NULL;
                        pAudioData->fileB = NULL;
                }
        }

        pAudioData->pUserData->currentSongData = currentSongData;

        pAudioData->currentPCMFrame = 0;

        setSeekElapsed(0.0);

        setEOFReached();
}

int getCurrentVolume()
{
        return soundVolume;
}

int getSystemVolume()
{
        FILE *fp;
        char command_str[1000];
        char buf[100];
        int currentVolume = -1;

        // Build the command string
        snprintf(command_str, sizeof(command_str),
                 "pactl get-sink-volume @DEFAULT_SINK@ | awk 'NR==1{print $5}'");

        // Execute the command and read the output
        fp = popen(command_str, "r");
        if (fp != NULL)
        {
                if (fgets(buf, sizeof(buf), fp) != NULL)
                {
                        sscanf(buf, "%d", &currentVolume);
                }
                pclose(fp);
        }

        return currentVolume;
}

void setVolume(int volume)
{
        if (volume > 100)
        {
                volume = 100;
        }
        else if (volume < 0)
        {
                volume = 0;
        }

        soundVolume = volume;

        ma_device_set_master_volume(getDevice(), (float)volume / 100);
}

int adjustVolumePercent(int volumeChange)
{
        int sysVol = getSystemVolume();

        if (sysVol == 0) 
                return 0;

        int step = 100 / sysVol * 5;

        int relativeVolChange = volumeChange / 5  * step;

        soundVolume += relativeVolChange;

        setVolume(soundVolume);

        return 0;
}
