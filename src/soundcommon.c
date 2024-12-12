#include "soundcommon.h"

/*

soundcommon.c

 Related to common functions for decoders / miniaudio implementations.

*/

#define MAX_DECODERS 2

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

bool repeatEnabled = false;
bool shuffleEnabled = false;
bool skipToNext = false;
bool seekRequested = false;
bool paused = false;
bool stopped = true;

bool hasSilentlySwitched;

float seekPercent = 0.0;
double seekElapsed;

_Atomic bool EOFReached = false;
_Atomic bool switchReached = false;
_Atomic bool readingFrames = false;
pthread_mutex_t dataSourceMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t switchMutex = PTHREAD_MUTEX_INITIALIZER;
ma_device device = {0};
ma_int32 *audioBuffer = NULL;
AudioData audioData;
int bufSize;
ma_event switchAudioImpl;
enum AudioImplementation currentImplementation = NONE;

AppState appState;
volatile bool refresh = true; // Should the whole view be refreshed next time it redraws
double duration;

double elapsedSeconds = 0.0;

int soundVolume = 100;

ma_decoder *firstDecoder;
ma_decoder *currentDecoder;

ma_decoder *decoders[MAX_DECODERS];
ma_libopus *opusDecoders[MAX_DECODERS];
ma_libopus *firstOpusDecoder;
ma_libvorbis *vorbisDecoders[MAX_DECODERS];
ma_libvorbis *firstVorbisDecoder;

#ifdef USE_FAAD
m4a_decoder *m4aDecoders[MAX_DECODERS];
m4a_decoder *firstM4aDecoder;
#endif

int decoderIndex = -1;
int m4aDecoderIndex = -1;
int opusDecoderIndex = -1;
int vorbisDecoderIndex = -1;

void logTime(const char *message)
{
        (void)message;
        // struct timespec ts;
        // clock_gettime(CLOCK_REALTIME, &ts);
        // printf("[%ld.%09ld] %s\n", ts.tv_sec, ts.tv_nsec, message);
}

enum AudioImplementation getCurrentImplementationType(void)
{
        return currentImplementation;
}

void setCurrentImplementationType(enum AudioImplementation value)
{
        currentImplementation = value;
}

ma_decoder *getFirstDecoder(void)
{
        return firstDecoder;
}

ma_decoder *getCurrentBuiltinDecoder(void)
{
        if (decoderIndex == -1)
                return getFirstDecoder();
        else
                return decoders[decoderIndex];
}

void switchDecoder(void)
{
        if (decoderIndex == -1)
                decoderIndex = 0;
        else
                decoderIndex = 1 - decoderIndex;
}

void resetDecoders(void)
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

void uninitPreviousDecoder(void)
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

void uninitPreviousVorbisDecoder(void)
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

#ifdef USE_FAAD
void uninitPreviousM4aDecoder(void)
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

m4a_decoder *getFirstM4aDecoder(void)
{
        return firstM4aDecoder;
}

m4a_decoder *getCurrentM4aDecoder(void)
{
        if (m4aDecoderIndex == -1)
                return getFirstM4aDecoder();
        else
                return m4aDecoders[m4aDecoderIndex];
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

void resetM4aDecoders(void)
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

MA_API ma_result m4a_read_pcm_frames_wrapper(void *pDecoder, void *pFramesOut, size_t frameCount, size_t *pFramesRead)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return m4a_decoder_read_pcm_frames((m4a_decoder *)dec->pUserData, pFramesOut, frameCount, (ma_uint64 *)pFramesRead);
}

MA_API ma_result m4a_seek_to_pcm_frame_wrapper(void *pDecoder, long long int frameIndex, ma_seek_origin origin)
{
        (void)origin;
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return m4a_decoder_seek_to_pcm_frame((m4a_decoder *)dec->pUserData, frameIndex);
}

MA_API ma_result m4a_get_cursor_in_pcm_frames_wrapper(void *pDecoder, long long int *pCursor)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return m4a_decoder_get_cursor_in_pcm_frames((m4a_decoder *)dec->pUserData, (ma_uint64 *)pCursor);
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
        if (currentDecoder != NULL && decoder != NULL)
        {
                if (!isEOFReached())
                        ma_data_source_set_next(currentDecoder, decoder);
        }
        return 0;
}

#endif

void uninitPreviousOpusDecoder(void)
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

ma_libvorbis *getFirstVorbisDecoder(void)
{
        return firstVorbisDecoder;
}

ma_libopus *getFirstOpusDecoder(void)
{
        return firstOpusDecoder;
}

ma_libvorbis *getCurrentVorbisDecoder(void)
{
        if (vorbisDecoderIndex == -1)
                return getFirstVorbisDecoder();
        else
                return vorbisDecoders[vorbisDecoderIndex];
}

ma_libopus *getCurrentOpusDecoder(void)
{
        if (opusDecoderIndex == -1)
                return getFirstOpusDecoder();
        else
                return opusDecoders[opusDecoderIndex];
}

ma_format getCurrentFormat(void)
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
#ifdef USE_FAAD
                m4a_decoder *decoder = getCurrentM4aDecoder();

                if (decoder != NULL)
                        format = decoder->format;
#endif
        }

        return format;
}

void switchVorbisDecoder(void)
{
        if (vorbisDecoderIndex == -1)
                vorbisDecoderIndex = 0;
        else
                vorbisDecoderIndex = 1 - vorbisDecoderIndex;
}

void switchM4aDecoder(void)
{
        if (m4aDecoderIndex == -1)
                m4aDecoderIndex = 0;
        else
                m4aDecoderIndex = 1 - m4aDecoderIndex;
}

void switchOpusDecoder(void)
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

void resetVorbisDecoders(void)
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

void resetOpusDecoders(void)
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

MA_API ma_result ma_libopus_read_pcm_frames_wrapper(void *pDecoder, void *pFramesOut, size_t frameCount, size_t *pFramesRead)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_libopus_read_pcm_frames((ma_libopus *)dec->pUserData, pFramesOut, frameCount, (ma_uint64 *)pFramesRead);
}

MA_API ma_result ma_libopus_seek_to_pcm_frame_wrapper(void *pDecoder, long long int frameIndex, ma_seek_origin origin)
{
        (void)origin;
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_libopus_seek_to_pcm_frame((ma_libopus *)dec->pUserData, frameIndex);
}

MA_API ma_result ma_libopus_get_cursor_in_pcm_frames_wrapper(void *pDecoder, long long int *pCursor)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_libopus_get_cursor_in_pcm_frames((ma_libopus *)dec->pUserData, (ma_uint64 *)pCursor);
}

MA_API ma_result ma_libvorbis_read_pcm_frames_wrapper(void *pDecoder, void *pFramesOut, size_t frameCount, size_t *pFramesRead)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_libvorbis_read_pcm_frames((ma_libvorbis *)dec->pUserData, pFramesOut, frameCount, (ma_uint64 *)pFramesRead);
}

MA_API ma_result ma_libvorbis_seek_to_pcm_frame_wrapper(void *pDecoder, long long int frameIndex, ma_seek_origin origin)
{
        (void)origin;
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_libvorbis_seek_to_pcm_frame((ma_libvorbis *)dec->pUserData, frameIndex);
}

MA_API ma_result ma_libvorbis_get_cursor_in_pcm_frames_wrapper(void *pDecoder, long long int *pCursor)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_libvorbis_get_cursor_in_pcm_frames((ma_libvorbis *)dec->pUserData, (ma_uint64 *)pCursor);
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
        if (currentDecoder != NULL && decoder != NULL)
        {
                if (!isEOFReached())
                        ma_data_source_set_next(currentDecoder, decoder);
        }
        return 0;
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

        if (currentDecoder != NULL && decoder != NULL)
        {
                if (!isEOFReached())
                        ma_data_source_set_next(currentDecoder, decoder);
        }
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

        if (currentDecoder != NULL && decoder != NULL)
        {
                if (!isEOFReached())
                        ma_data_source_set_next(currentDecoder, decoder);
        }
        return 0;
}

int getBufferSize(void)
{
        return bufSize;
}

void setBufferSize(int value)
{
        bufSize = value;
}

void initAudioBuffer(void)
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

ma_int32 *getAudioBuffer(void)
{
        return audioBuffer;
}

void setAudioBuffer(ma_int32 *buf)
{
        audioBuffer = buf;
}

void resetAudioBuffer(void)
{
        memset(audioBuffer, 0, sizeof(float) * MAX_BUFFER_SIZE);
}

void freeAudioBuffer(void)
{
        if (audioBuffer != NULL)
        {
                free(audioBuffer);
                audioBuffer = NULL;
        }
}

bool isRepeatEnabled(void)
{
        return repeatEnabled;
}

void setRepeatEnabled(bool value)
{
        repeatEnabled = value;
}

bool isShuffleEnabled(void)
{
        return shuffleEnabled;
}

void setShuffleEnabled(bool value)
{
        shuffleEnabled = value;
}

bool isSkipToNext(void)
{
        return skipToNext;
}

void setSkipToNext(bool value)
{
        skipToNext = value;
}

double getSeekElapsed(void)
{
        return seekElapsed;
}

double getPercentageElapsed(void)
{
        return elapsedSeconds / duration;
}

void setSeekElapsed(double value)
{
        seekElapsed = value;
}

bool isEOFReached(void)
{
        return atomic_load(&EOFReached);
}

void setEOFReached(void)
{
        atomic_store(&EOFReached, true);
}

void setEOFNotReached(void)
{
        atomic_store(&EOFReached, false);
}

bool isImplSwitchReached(void)
{
        return atomic_load(&switchReached) ? true : false;
}

void setImplSwitchReached(void)
{
        atomic_store(&switchReached, true);
}

void setImplSwitchNotReached(void)
{
        atomic_store(&switchReached, false);
}

bool isPlaying(void)
{
        return ma_device_is_started(&device);
}

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

float getSeekPercentage(void)
{
        return seekPercent;
}

bool isSeekRequested(void)
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

void resumePlayback(void)
{
        // if this was unpaused with no song loaded
        if (audioData.restart)
        {
                audioData.endOfListReached = false;
        }

        if (!ma_device_is_started(&device))
        {
                ma_device_start(&device);
        }

        paused = false;

        stopped = false;

        if (appState.currentView != SONG_VIEW)
        {
                refresh = true;
        }
}

void stopPlayback(void)
{
        if (ma_device_is_started(&device))
        {
                ma_device_stop(&device);
        }

        stopped = true;

        if (appState.currentView != SONG_VIEW)
        {
                refresh = true;
        }
}

void pausePlayback(void)
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

void cleanupPlaybackDevice(void)
{
        ma_device_stop(&device);
        while (ma_device_get_state(&device) != ma_device_state_stopped && ma_device_get_state(&device) != ma_device_state_uninitialized)
        {
                c_sleep(100);
        }
        ma_device_uninit(&device);
}

void clearCurrentTrack(void)
{
        if (ma_device_is_started(&device))
        {
                // Stop the device (which stops playback)
                ma_device_stop(&device);
        }

        resetDecoders();
        resetVorbisDecoders();
        resetOpusDecoders();
#ifdef USE_FAAD
        resetM4aDecoders();
#endif
        ma_data_source_set_next(currentDecoder, NULL);
}

void togglePausePlayback(void)
{
        if (ma_device_is_started(&device))
        {
                pausePlayback();
        }
        else if (isPaused() || isStopped())
        {
                resumePlayback();
        }
}

bool isPaused(void)
{
        return paused;
}

bool isStopped(void)
{
        return stopped;
}

pthread_mutex_t deviceMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t deviceStopped = PTHREAD_COND_INITIALIZER;

void resetDevice(void)
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

ma_device *getDevice(void)
{
        return &device;
}

bool hasBuiltinDecoder(char *filePath)
{
        char *extension = strrchr(filePath, '.');
        return (extension != NULL && (strcasecmp(extension, ".wav") == 0 || strcasecmp(extension, ".flac") == 0 ||
                                      strcasecmp(extension, ".mp3") == 0));
}

void setCurrentFileIndex(AudioData *pAudioData, int index)
{
        pthread_mutex_lock(&switchMutex);
        pAudioData->currentFileIndex = index;
        pthread_mutex_unlock(&switchMutex);
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

gint64 getLengthInMicroSec(double duration)
{
        return floor(llround(duration * G_USEC_PER_SEC));
}

void executeSwitch(AudioData *pAudioData)
{
        pAudioData->switchFiles = false;
        switchDecoder();
        switchOpusDecoder();
        switchM4aDecoder();
        switchVorbisDecoder();

        pAudioData->pUserData->currentSongData = (pAudioData->currentFileIndex == 0) ? pAudioData->pUserData->songdataA : pAudioData->pUserData->songdataB;
        pAudioData->totalFrames = 0;
        pAudioData->currentPCMFrame = 0;

        setSeekElapsed(0.0);

        setEOFReached();
}

int getCurrentVolume(void)
{
        return soundVolume;
}

int extractPercentage(char *str)
{
        int volume = -1;
        char *percentSign = strchr(str, '%');
        if (percentSign != NULL)
        {
                // Find the start of the number before the '%'
                while (percentSign > str && *(percentSign - 1) == ' ')
                        percentSign--;
                while (percentSign > str && *(percentSign - 1) >= '0' && *(percentSign - 1) <= '9')
                        percentSign--;

                volume = getNumber(percentSign);
        }
        return volume;
}

int extractPercentageMac(const char *buf)
{
        int volume;
        if (sscanf(buf, "%d", &volume) == 1)
        {
                return volume;
        }
        return -1;
}

int getSystemVolumeMac(void)
{
        FILE *fp;
        char command_str[1000];
        char buf[256];
        int currentVolume = -1;

        // macOS uses AppleScript to get the system volume
        snprintf(command_str, sizeof(command_str),
                 "osascript -e 'output volume of (get volume settings)'");

        fp = popen(command_str, "r");
        if (fp != NULL)
        {
                if (fgets(buf, sizeof(buf), fp) != NULL)
                {
                        buf[strcspn(buf, "\n")] = '\0';
                        int tempVolume = extractPercentageMac(buf);
                        if (tempVolume != -1)
                        {
                                currentVolume = tempVolume;
                        }
                }
                pclose(fp);
        }

        return currentVolume;
}

int getSystemVolume(void)
{
#ifdef __APPLE__
        return getSystemVolumeMac();
#else
        FILE *fp;
        char command_str[1000];
        char buf[256];
        int currentVolume = -1;

        // Use '@DEFAULT_SINK@' to get the default sink's volume directly
        snprintf(command_str, sizeof(command_str),
                 "pactl get-sink-volume @DEFAULT_SINK@");

        fp = popen(command_str, "r");
        if (fp != NULL)
        {
                while (fgets(buf, sizeof(buf), fp) != NULL)
                {
                        int tempVolume = extractPercentage(buf);
                        if (tempVolume != -1)
                        {
                                currentVolume = tempVolume;
                                break;
                        }
                }
                pclose(fp);
        }

        // ALSA fallback if `pactl` fails
        if (currentVolume == -1)
        {
                snprintf(command_str, sizeof(command_str),
                         "amixer get Master");

                fp = popen(command_str, "r");
                if (fp != NULL)
                {
                        while (fgets(buf, sizeof(buf), fp) != NULL)
                        {
                                int tempVolume = extractPercentage(buf);
                                if (tempVolume != -1)
                                {
                                        currentVolume = tempVolume;
                                        break;
                                }
                        }
                        pclose(fp);
                }
        }

        return currentVolume;
#endif
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

        int relativeVolChange = volumeChange / 5 * step;

        soundVolume += relativeVolChange;

        setVolume(soundVolume);

        return 0;
}

ma_uint64 lastCursor = 0;

#ifdef USE_FAAD
void m4a_read_pcm_frames(ma_data_source *pDataSource, void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead)
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
                        ma_data_source_get_length_in_pcm_frames(decoder, &pAudioData->totalFrames);

                // Check if seeking is requested
                if (isSeekRequested())
                {
                        ma_uint64 totalFrames = pAudioData->totalFrames;
                        ma_uint64 seekPercent = getSeekPercentage();

                        if (seekPercent >= 100.0)
                                seekPercent = 100.0;

                        ma_uint64 targetFrame = (ma_uint64)((totalFrames - 1) * seekPercent / 100.0);

                        if (targetFrame >= totalFrames)
                                targetFrame = totalFrames - 1;

                        // Set the read pointer for the decoder
                        ma_result seekResult = m4a_decoder_seek_to_pcm_frame(decoder, targetFrame);
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
                m4a_decoder *firstDecoder = getFirstM4aDecoder();
                ma_uint64 cursor = 0;

                if (firstDecoder == NULL)
                {
                        pthread_mutex_unlock(&dataSourceMutex);
                        return;
                }

                result = ma_data_source_read_pcm_frames(firstDecoder, (ma_int32 *)pFramesOut + framesRead * pAudioData->channels, remainingFrames, &framesToRead);

                ma_data_source_get_cursor_in_pcm_frames(decoder, &cursor);

                if (((cursor != 0 && cursor == lastCursor) || framesToRead == 0 || isSkipToNext() || result != MA_SUCCESS) && !isEOFReached())
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

void m4a_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount)
{
        AudioData *pDataSource = (AudioData *)pDevice->pUserData;
        ma_uint64 framesRead = 0;
        m4a_read_pcm_frames(&pDataSource->base, pFramesOut, frameCount, &framesRead);
        (void)pFramesIn;
}
#endif

void opus_read_pcm_frames(ma_data_source *pDataSource, void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead)
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
                        ma_data_source_get_length_in_pcm_frames(decoder, &pAudioData->totalFrames);

                // Check if seeking is requested
                if (isSeekRequested())
                {
                        ma_uint64 totalFrames = 0;
                        ma_libopus_get_length_in_pcm_frames(decoder, &totalFrames);
                        ma_uint64 seekPercent = getSeekPercentage();
                        if (seekPercent >= 100.0)
                                seekPercent = 100.0;
                        ma_uint64 targetFrame = (ma_uint64)((totalFrames - 1) * seekPercent / 100.0);

                        if (targetFrame >= totalFrames)
                                targetFrame = totalFrames - 1;

                        // Set the read pointer for the decoder
                        ma_result seekResult = ma_libopus_seek_to_pcm_frame(decoder, targetFrame);
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

                result = ma_data_source_read_pcm_frames(firstDecoder, (ma_int32 *)pFramesOut + framesRead * pAudioData->channels, remainingFrames, &framesToRead);

                ma_data_source_get_cursor_in_pcm_frames(decoder, &cursor);

                if (((cursor != 0 && cursor >= pAudioData->totalFrames) || framesToRead == 0 || isSkipToNext() || result != MA_SUCCESS) && !isEOFReached())
                {
                        activateSwitch(pAudioData);
                        pthread_mutex_unlock(&dataSourceMutex);
                        continue;
                }

                framesRead += framesToRead;
                setBufferSize(framesToRead);

                pthread_mutex_unlock(&dataSourceMutex);
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
        AudioData *pDataSource = (AudioData *)pDevice->pUserData;
        ma_uint64 framesRead = 0;
        opus_read_pcm_frames(&pDataSource->base, pFramesOut, frameCount, &framesRead);
        (void)pFramesIn;
}

void vorbis_read_pcm_frames(ma_data_source *pDataSource, void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead)
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
                        break; // Exit the loop after the file switch
                }

                ma_libvorbis *decoder = getCurrentVorbisDecoder();

                if ((getCurrentImplementationType() != VORBIS && !isSkipToNext()) || (decoder == NULL))
                {
                        pthread_mutex_unlock(&dataSourceMutex);
                        return;
                }

                if (isSeekRequested())
                {
                        // disabled for ogg vorbis
                        setSeekRequested(false);
                }

                // Read from the current decoder
                ma_uint64 framesToRead = 0;
                ma_result result;
                ma_uint64 remainingFrames = frameCount - framesRead;
                ma_libvorbis *firstDecoder = getFirstVorbisDecoder();

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

                result = ma_data_source_read_pcm_frames(firstDecoder, (ma_int32 *)pFramesOut + framesRead * pAudioData->channels, remainingFrames, &framesToRead);

                if ((getPercentageElapsed() >= 1.0 || isSkipToNext() || result != MA_SUCCESS) &&
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

void vorbis_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount)
{
        AudioData *pDataSource = (AudioData *)pDevice->pUserData;
        ma_uint64 framesRead = 0;
        vorbis_read_pcm_frames(&pDataSource->base, pFramesOut, frameCount, &framesRead);
        (void)pFramesIn;
}
