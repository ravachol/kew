#include "soundcommon.h"
#include "playerops.h"

/*

soundcommon.c

 Related to common functions for decoders / miniaudio implementations.

*/

#define MAX_DECODERS 2

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

bool repeatEnabled = false;
bool repeatListEnabled = false;
bool shuffleEnabled = false;
bool skipToNext = false;
bool seekRequested = false;
bool paused = false;
bool stopped = true;

bool hasSilentlySwitched;

int hopSize = 512;
int fftSize = 2048;
int prevFftSize = 0;
int fftSizeMilliseconds = 45;

float seekPercent = 0.0;
double seekElapsed;

_Atomic bool EOFReached = false;
_Atomic bool switchReached = false;
_Atomic bool readingFrames = false;
pthread_mutex_t dataSourceMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t switchMutex = PTHREAD_MUTEX_INITIALIZER;
ma_device device = {0};
static float audioBuffer[MAX_BUFFER_SIZE];
static int writeHead = 0;
bool bufferReady = false;
AudioData audioData;
int bufSize;
ma_event switchAudioImpl;
enum AudioImplementation currentImplementation = NONE;

AppState appState;

double elapsedSeconds = 0.0;

int soundVolume = 100;

ma_decoder *firstDecoder;
ma_decoder *currentDecoder;

ma_decoder *decoders[MAX_DECODERS];
ma_libopus *opusDecoders[MAX_DECODERS];
ma_libopus *firstOpusDecoder;
ma_libvorbis *vorbisDecoders[MAX_DECODERS];
ma_libvorbis *firstVorbisDecoder;
ma_webm *webmDecoders[MAX_DECODERS];
ma_webm *firstWebmDecoder;

#ifdef USE_FAAD
m4a_decoder *m4aDecoders[MAX_DECODERS];
m4a_decoder *firstM4aDecoder;
#endif

int decoderIndex = -1;
int m4aDecoderIndex = -1;
int opusDecoderIndex = -1;
int vorbisDecoderIndex = -1;
int webmDecoderIndex = -1;

void uninitMaDecoder(void *decoder)
{
        ma_decoder_uninit((ma_decoder *)decoder);
}

void uninitOpusDecoder(void *decoder)
{
        ma_libopus_uninit((ma_libopus *)decoder, NULL);
}

void uninitVorbisDecoder(void *decoder)
{
        ma_libvorbis_uninit((ma_libvorbis *)decoder, NULL);
}

void uninitWebmDecoder(void *decoder)
{
        ma_webm_uninit((ma_webm *)decoder, NULL);
}

#ifdef USE_FAAD
void uninitM4aDecoder(void *decoder)
{
        m4a_decoder_uninit((m4a_decoder *)decoder, NULL);
}
#endif

void uninitPreviousDecoder(void **decoderArray, int index, uninit_func uninit)
{
        if (index == -1)
        {
                return;
        }

        void *toUninit = decoderArray[1 - index];

        if (toUninit != NULL)
        {
                uninit(toUninit);
                free(toUninit);
                decoderArray[1 - index] = NULL;
        }
}

void resetDecoders(void **decoderArray, void **firstDecoder, int arraySize, int *decoderIndex, uninit_func uninit)
{
        *decoderIndex = -1;

        if (*firstDecoder != NULL)
        {
                uninit(*firstDecoder);
                free(*firstDecoder);
                *firstDecoder = NULL;
        }

        for (int i = 0; i < arraySize; i++)
        {
                if (decoderArray[i] != NULL)
                {
                        uninit(decoderArray[i]);
                        free(decoderArray[i]);
                        decoderArray[i] = NULL;
                }
        }
}

void resetAllDecoders()
{
        resetDecoders((void **)decoders, (void **)&firstDecoder, MAX_DECODERS, &decoderIndex, uninitMaDecoder);
        resetDecoders((void **)vorbisDecoders, (void **)&firstVorbisDecoder, MAX_DECODERS, &vorbisDecoderIndex, uninitVorbisDecoder);
        resetDecoders((void **)opusDecoders, (void **)&firstOpusDecoder, MAX_DECODERS, &opusDecoderIndex, uninitOpusDecoder);
        resetDecoders((void **)webmDecoders, (void **)&firstWebmDecoder, MAX_DECODERS, &webmDecoderIndex, uninitWebmDecoder);
#ifdef USE_FAAD
        resetDecoders((void **)m4aDecoders, (void **)&firstM4aDecoder, MAX_DECODERS, &m4aDecoderIndex, uninitM4aDecoder);
#endif
}

void setNextDecoder(void **decoderArray, void **decoder, void **firstDecoder, int *decoderIndex, uninit_func uninit)
{
        if (*decoderIndex == -1 && *firstDecoder == NULL)
        {
                *firstDecoder = *decoder;
        }
        else if (*decoderIndex == -1) // Array hasn't been used yet
        {
                if (decoderArray[0] != NULL)
                {
                        uninit(decoderArray[0]);
                        free(decoderArray[0]);
                        decoderArray[0] = NULL;
                }

                decoderArray[0] = *decoder;
        }
        else
        {
                int nextIndex = 1 - *decoderIndex;
                if (decoderArray[nextIndex] != NULL)
                {
                        uninit(decoderArray[nextIndex]);
                        free(decoderArray[nextIndex]);
                        decoderArray[nextIndex] = NULL;
                }

                decoderArray[nextIndex] = *decoder;
        }
}

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

void switchDecoder(int *decoderIndex)
{
        if (*decoderIndex == -1)
                *decoderIndex = 0;
        else
                *decoderIndex = 1 - *decoderIndex;
}

#ifdef USE_FAAD
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

void getM4aFileInfo(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sampleRate, ma_channel *channelMap, int *avgBitRate, k_m4adec_filetype *fileType)
{
        m4a_decoder decoder;
        if (m4a_decoder_init_file(filename, NULL, NULL, &decoder) == MA_SUCCESS)
        {
                *format = decoder.format;
                m4a_decoder_get_data_format(&decoder, format, channels, sampleRate, channelMap, MA_MAX_CHANNELS);
                *avgBitRate = decoder.avgBitRate / 1000;
                *fileType = decoder.fileType;
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

int prepareNextM4aDecoder(SongData *songData)
{
        m4a_decoder *currentDecoder;

        if (songData == NULL)
                return -1;

        char *filepath = songData->filePath;

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

        uninitPreviousDecoder((void **)m4aDecoders, m4aDecoderIndex, (uninit_func)uninitM4aDecoder);

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
                                                      sampleRate == nsampleRate &&
                                                      currentDecoder->fileType == decoder->fileType &&
                                                      currentDecoder->fileType != k_rawAAC));

        if (!sameFormat)
        {
                m4a_decoder_uninit(decoder, NULL);
                free(decoder);
                return 0;
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

        setNextDecoder((void **)m4aDecoders, (void **)&decoder, (void **)&firstM4aDecoder, &m4aDecoderIndex, (uninit_func)uninitM4aDecoder);

        if (songData != NULL)
        {
                if (decoder != NULL && decoder->fileType == k_rawAAC)
                {
                        songData->duration = decoder->duration;
                }
        }

        if (currentDecoder != NULL && decoder != NULL && decoder->fileType != k_rawAAC)
        {
                if (!isEOFReached())
                        ma_data_source_set_next(currentDecoder, decoder);
        }
        return 0;
}

#endif

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

        *sampleRate = audioData.sampleRate;
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

MA_API ma_result ma_webm_read_pcm_frames_wrapper(void *pDecoder, void *pFramesOut, size_t frameCount, size_t *pFramesRead)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_webm_read_pcm_frames((ma_webm *)dec->pUserData, pFramesOut, frameCount, (ma_uint64 *)pFramesRead);
}

MA_API ma_result ma_webm_seek_to_pcm_frame_wrapper(void *pDecoder, long long int frameIndex, ma_seek_origin origin)
{
        (void)origin;
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_webm_seek_to_pcm_frame((ma_webm *)dec->pUserData, frameIndex);
}

MA_API ma_result ma_webm_get_cursor_in_pcm_frames_wrapper(void *pDecoder, long long int *pCursor)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_webm_get_cursor_in_pcm_frames((ma_webm *)dec->pUserData, (ma_uint64 *)pCursor);
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

        uninitPreviousDecoder((void **)vorbisDecoders, vorbisDecoderIndex, (uninit_func)uninitVorbisDecoder);

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
                return 0;
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

        setNextDecoder((void **)vorbisDecoders, (void **)&decoder, (void **)&firstVorbisDecoder, &vorbisDecoderIndex, (uninit_func)uninitVorbisDecoder);

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

        uninitPreviousDecoder((void **)decoders, decoderIndex, (uninit_func)uninitMaDecoder);

        ma_decoder *decoder = (ma_decoder *)malloc(sizeof(ma_decoder));
        ma_result result = ma_decoder_init_file(filepath, NULL, decoder);

        if (result != MA_SUCCESS)
        {
                free(decoder);
                return -1;
        }
        setNextDecoder((void **)decoders, (void **)&decoder, (void **)&firstDecoder, &decoderIndex, (uninit_func)uninitMaDecoder);

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
                currentDecoder = opusDecoders[opusDecoderIndex];
        }

        ma_uint32 sampleRate;
        ma_uint32 channels;
        ma_format format;
        ma_channel channelMap[MA_MAX_CHANNELS];
        ma_libopus_get_data_format(currentDecoder, &format, &channels, &sampleRate, channelMap, MA_MAX_CHANNELS);

        uninitPreviousDecoder((void **)opusDecoders, opusDecoderIndex, (uninit_func)uninitOpusDecoder);

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
                return 0;
        }

        if (firstOpusDecoder != NULL)
        {
                decoder->pReadSeekTellUserData = (AudioData *)firstOpusDecoder->pReadSeekTellUserData;
        }

        decoder->format = nformat;
        decoder->onRead = ma_libopus_read_pcm_frames_wrapper;
        decoder->onSeek = ma_libopus_seek_to_pcm_frame_wrapper;
        decoder->onTell = ma_libopus_get_cursor_in_pcm_frames_wrapper;

        setNextDecoder((void **)opusDecoders, (void **)&decoder, (void **)&firstOpusDecoder, &opusDecoderIndex, (uninit_func)uninitOpusDecoder);

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

int closestPowerOfTwo(int x)
{
        int n = 1;
        while (n < x)
                n <<= 1;
        return n;
}

// Sign-extend s24
ma_int32 unpack_s24(const ma_uint8 *p)
{
        ma_int32 sample = p[0] | (p[1] << 8) | (p[2] << 16);
        if (sample & 0x800000)
                sample |= ~0xFFFFFF;
        return sample;
}

void setAudioBuffer(
    void *buf,
    int numFrames,
    ma_uint32 sampleRate,
    ma_uint32 channels,
    ma_format format)
{
        int bufIndex = 0;

        // Dynamically determine FFT and hop size
        float hopFraction = 0.25f; // 25% hop (75% overlap)

        // Compute power-of-two window/hop sizes in samples
        int wantFFTSamples = (int)(fftSizeMilliseconds * sampleRate / 1000.0f);
        fftSize = closestPowerOfTwo(wantFFTSamples);       // 2048 or 4096
        int wantHopSamples = (int)(fftSize * hopFraction); // 25% of window length
        hopSize = closestPowerOfTwo(wantHopSamples);       // 256, 512, 1024

        if (fftSize > MAX_BUFFER_SIZE)
                fftSize = MAX_BUFFER_SIZE;

        // Ensure hop is never >= window
        if (hopSize >= fftSize)
                hopSize = fftSize / 2; // fallback minimum overlap

        while (bufIndex < numFrames)
        {

                if (writeHead >= fftSize)
                        break;

                int framesLeft = numFrames - bufIndex;
                int spaceLeft = fftSize - writeHead;
                int framesToCopy = framesLeft < spaceLeft ? framesLeft : spaceLeft;

                switch (format)
                {
                case ma_format_u8:
                {
                        ma_uint8 *src = (ma_uint8 *)buf + bufIndex * channels;
                        for (int i = 0; i < framesToCopy; ++i)
                        {
                                float sum = 0.0f;
                                for (ma_uint32 ch = 0; ch < channels; ++ch)
                                {
                                        // Convert 0..255 to -1..1
                                        sum += ((float)src[i * channels + ch] - 128.0f) / 128.0f;
                                }
                                audioBuffer[writeHead++] = sum / channels;
                        }
                        break;
                }
                case ma_format_s16:
                {
                        ma_int16 *src = (ma_int16 *)buf + bufIndex * channels;
                        for (int i = 0; i < framesToCopy; ++i)
                        {
                                float sum = 0.0f;
                                for (ma_uint32 ch = 0; ch < channels; ++ch)
                                {
                                        sum += (float)src[i * channels + ch] / 32768.0f;
                                }
                                audioBuffer[writeHead++] = sum / channels;
                        }
                        break;
                }
                case ma_format_s24:
                {
                        ma_uint8 *src = (ma_uint8 *)buf + bufIndex * channels * 3;
                        for (int i = 0; i < framesToCopy; ++i)
                        {
                                float sum = 0.0f;
                                for (ma_uint32 ch = 0; ch < channels; ++ch)
                                {
                                        int idx = i * channels * 3 + ch * 3;
                                        int32_t s = unpack_s24(&src[idx]);
                                        sum += (float)s / 8388608.0f;
                                }
                                audioBuffer[writeHead++] = sum / channels;
                        }
                        break;
                }
                case ma_format_s32:
                {
                        int32_t *src = (int32_t *)buf + bufIndex * channels;
                        for (int i = 0; i < framesToCopy; ++i)
                        {
                                float sum = 0.0f;
                                for (ma_uint32 ch = 0; ch < channels; ++ch)
                                {
                                        sum += (float)src[i * channels + ch] / 2147483648.0f;
                                }
                                audioBuffer[writeHead++] = sum / channels;
                        }
                        break;
                }
                case ma_format_f32:
                {
                        float *src = (float *)buf + bufIndex * channels;
                        for (int i = 0; i < framesToCopy; ++i)
                        {
                                float sum = 0.0f;
                                for (ma_uint32 ch = 0; ch < channels; ++ch)
                                {
                                        sum += src[i * channels + ch];
                                }
                                audioBuffer[writeHead++] = sum / channels;
                        }
                        break;
                }
                default:
                        fprintf(stderr, "Unsupported format in setAudioBuffer!\n");
                        return;
                }
                bufIndex += framesToCopy;

                // Process full window(s), maintain overlap (hop)
                while (writeHead >= fftSize)
                {
                        bufferReady = true; // let main loop know FFT is ready

                        // Shift buffer for overlap (keep last fftSize-hopSize samples)
                        memmove(
                            audioBuffer,
                            audioBuffer + hopSize,
                            sizeof(float) * (fftSize - hopSize));
                        writeHead -= hopSize;
                }
        }
}

void resetAudioBuffer(void)
{
        memset(audioBuffer, 0, sizeof(ma_int32) * MAX_BUFFER_SIZE);
        writeHead = 0;
        bufferReady = false;
}

void *getAudioBuffer(void)
{
        return audioBuffer;
}

bool isRepeatEnabled(void)
{
        return repeatEnabled;
}

void setRepeatEnabled(bool value)
{
        repeatEnabled = value;
}

bool isRepeatListEnabled(void)
{
        return repeatListEnabled;
}

void setRepeatListEnabled(bool value)
{
        repeatListEnabled = value;
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

void stopPlayback(void)
{
        if (ma_device_is_started(&device))
        {
                ma_device_stop(&device);
        }

        stopped = true;

        if (appState.currentView != TRACK_VIEW)
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

        if (appState.currentView != TRACK_VIEW)
        {
                refresh = true;
        }
}

void cleanupPlaybackDevice(void)
{
        ma_device_uninit(&device);
        memset(&device, 0, sizeof(device));
}

void clearCurrentTrack(void)
{
        if (ma_device_is_started(&device))
        {
                // Stop the device (which stops playback)
                ma_device_stop(&device);
        }

        resetAllDecoders();
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
                if (isStopped())
                {
                        resetClock();
                }
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
        switchDecoder(&decoderIndex);
        switchDecoder(&opusDecoderIndex);
        switchDecoder(&m4aDecoderIndex);
        switchDecoder(&vorbisDecoderIndex);
        switchDecoder(&webmDecoderIndex);

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
        soundVolume += volumeChange;

        setVolume(soundVolume);

        return 0;
}

ma_uint64 lastCursor = 0;

ma_result callReadPCMFrames(
    ma_data_source *pDataSource,
    ma_format format,
    void *pFramesOut,
    ma_uint64 framesRead,
    ma_uint32 channels,
    ma_uint64 remainingFrames,
    ma_uint64 *pFramesToRead)
{
        ma_result result;

        switch (format)
        {
        case ma_format_u8:
        {
                ma_uint8 *pOut = (ma_uint8 *)pFramesOut;
                result = ma_data_source_read_pcm_frames(
                    pDataSource,
                    pOut + (framesRead * channels),
                    remainingFrames,
                    pFramesToRead);
        }
        break;

        case ma_format_s16:
        {
                ma_int16 *pOut = (ma_int16 *)pFramesOut;
                result = ma_data_source_read_pcm_frames(
                    pDataSource,
                    pOut + (framesRead * channels),
                    remainingFrames,
                    pFramesToRead);
        }
        break;

        case ma_format_s24:
        {
                ma_uint8 *pOut = (ma_uint8 *)pFramesOut;
                result = ma_data_source_read_pcm_frames(
                    pDataSource,
                    pOut + (framesRead * channels * 3),
                    remainingFrames,
                    pFramesToRead);
        }
        break;

        case ma_format_s32:
        {
                ma_int32 *pOut = (ma_int32 *)pFramesOut;
                result = ma_data_source_read_pcm_frames(
                    pDataSource,
                    pOut + (framesRead * channels),
                    remainingFrames,
                    pFramesToRead);
        }
        break;

        case ma_format_f32:
        {
                float *pOut = (float *)pFramesOut;
                result = ma_data_source_read_pcm_frames(
                    pDataSource,
                    pOut + (framesRead * channels),
                    remainingFrames,
                    pFramesToRead);
        }
        break;

        default:
        {
                result = MA_INVALID_ARGS;
        }
        break;
        }

        return result;
}

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
                        ma_data_source_get_length_in_pcm_frames(decoder, &(pAudioData->totalFrames));

                // Check if seeking is requested
                if (isSeekRequested())
                {
                        if (decoder->fileType != k_rawAAC)
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

                result = callReadPCMFrames(
                    firstDecoder,
                    m4a->format,
                    pFramesOut,
                    framesRead,
                    pAudioData->channels,
                    remainingFrames,
                    &framesToRead);

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

        setAudioBuffer(pFramesOut, framesRead, pAudioData->sampleRate, pAudioData->channels, pAudioData->format);

        if (pFramesRead != NULL)
        {
                *pFramesRead = framesRead;
        }
}

void m4a_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount)
{
        AudioData *pDataSource = (AudioData *)pDevice->pUserData;
        ma_uint64 framesRead = 0;
        m4a_read_pcm_frames(&(pDataSource->base), pFramesOut, frameCount, &framesRead);
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
                        ma_data_source_get_length_in_pcm_frames(decoder, &(pAudioData->totalFrames));

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

                result = callReadPCMFrames(
                    firstDecoder,
                    opus->format,
                    pFramesOut,
                    framesRead,
                    pAudioData->channels,
                    remainingFrames,
                    &framesToRead);

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

        setAudioBuffer(pFramesOut, framesRead, pAudioData->sampleRate, pAudioData->channels, pAudioData->format);

        if (pFramesRead != NULL)
        {
                *pFramesRead = framesRead;
        }
}

void opus_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount)
{
        AudioData *pDataSource = (AudioData *)pDevice->pUserData;
        ma_uint64 framesRead = 0;
        opus_read_pcm_frames(&(pDataSource->base), pFramesOut, frameCount, &framesRead);
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
                        break;
                }

                ma_libvorbis *decoder = getCurrentVorbisDecoder();

                if (pAudioData->totalFrames == 0)
                        ma_data_source_get_length_in_pcm_frames(decoder, &(pAudioData->totalFrames));

                if ((getCurrentImplementationType() != VORBIS && !isSkipToNext()) || (decoder == NULL))
                {
                        pthread_mutex_unlock(&dataSourceMutex);
                        return;
                }

                // Check if seeking is requested
                if (isSeekRequested())
                {
                        ma_uint64 totalFrames = 0;
                        ma_libvorbis_get_length_in_pcm_frames(decoder, &totalFrames);
                        ma_uint64 seekPercent = getSeekPercentage();
                        if (seekPercent >= 100.0)
                                seekPercent = 100.0;
                        ma_uint64 targetFrame = (ma_uint64)((totalFrames - 1) * seekPercent / 100.0);

                        if (targetFrame >= totalFrames)
                                targetFrame = totalFrames - 1;

                        // Set the read pointer for the decoder
                        ma_result seekResult = ma_libvorbis_seek_to_pcm_frame(decoder, targetFrame);
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
                    firstDecoder,
                    vorbis->format,
                    pFramesOut,
                    framesRead,
                    pAudioData->channels,
                    framesRequested,
                    &framesToRead);

                ma_data_source_get_cursor_in_pcm_frames(decoder, &cursor);

                if (((cursor != 0 && cursor >= pAudioData->totalFrames) || isSkipToNext() || result != MA_SUCCESS) &&
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

        setAudioBuffer(pFramesOut, framesRead, pAudioData->sampleRate, pAudioData->channels, pAudioData->format);

        if (pFramesRead != NULL)
        {
                *pFramesRead = framesRead;
        }
}

void vorbis_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount)
{
        AudioData *pDataSource = (AudioData *)pDevice->pUserData;
        ma_uint64 framesRead = 0;
        vorbis_read_pcm_frames(&(pDataSource->base), pFramesOut, frameCount, &framesRead);
        (void)pFramesIn;
}

void webm_read_pcm_frames(ma_data_source *pDataSource, void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead)
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
                        ma_data_source_get_length_in_pcm_frames(decoder, &(pAudioData->totalFrames));

                if ((getCurrentImplementationType() != WEBM && !isSkipToNext()) || (decoder == NULL))
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
                        ma_uint64 targetFrame = (ma_uint64)((totalFrames - 1) * seekPercent / 100.0);

                        if (targetFrame >= totalFrames)
                                targetFrame = totalFrames - 1;

                        // Set the read pointer for the decoder
                        ma_result seekResult = ma_webm_seek_to_pcm_frame(decoder, targetFrame);
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
                    firstDecoder,
                    webm->format,
                    pFramesOut,
                    framesRead,
                    pAudioData->channels,
                    framesRequested,
                    &framesToRead);

                ma_data_source_get_cursor_in_pcm_frames(decoder, &cursor);

                if (((cursor != 0 && cursor >= pAudioData->totalFrames) || isSkipToNext() || result != MA_SUCCESS) &&
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

        setAudioBuffer(pFramesOut, framesRead, pAudioData->sampleRate, pAudioData->channels, pAudioData->format);

        if (pFramesRead != NULL)
        {
                *pFramesRead = framesRead;
        }
}

int prepareNextWebmDecoder(SongData *songData)
{
        ma_webm *currentDecoder;

        if (songData == NULL)
                return -1;

        char *filepath = songData->filePath;

        if (webmDecoderIndex == -1)
        {
                currentDecoder = getFirstWebmDecoder();
        }
        else
        {
                currentDecoder = webmDecoders[webmDecoderIndex];
        }

        ma_uint32 sampleRate;
        ma_uint32 channels;
        ma_format format;
        ma_channel channelMap[MA_MAX_CHANNELS];
        ma_webm_get_data_format(currentDecoder, &format, &channels, &sampleRate, channelMap, MA_MAX_CHANNELS);

        uninitPreviousDecoder((void **)webmDecoders, webmDecoderIndex, (uninit_func)uninitWebmDecoder);

        ma_webm *decoder = (ma_webm *)malloc(sizeof(ma_webm));
        ma_result result = ma_webm_init_file(filepath, NULL, NULL, decoder);

        if (result != MA_SUCCESS)
                return -1;

        ma_format nformat;
        ma_uint32 nchannels;
        ma_uint32 nsampleRate;
        ma_channel nchannelMap[MA_MAX_CHANNELS];

        ma_webm_get_data_format(decoder, &nformat, &nchannels, &nsampleRate, nchannelMap, MA_MAX_CHANNELS);

        bool sameFormat = (currentDecoder == NULL);

        // FIXME gapless playback disabled for webm
        // bool sameFormat = (currentDecoder == NULL || (format == nformat &&
        //                                              channels == nchannels &&
        //                                              sampleRate == nsampleRate));

        if (!sameFormat)
        {
                ma_webm_uninit(decoder, NULL);
                free(decoder);
                return 0;
        }

        if (firstWebmDecoder != NULL)
        {
                decoder->pReadSeekTellUserData = (AudioData *)firstWebmDecoder->pReadSeekTellUserData;
        }

        decoder->format = nformat;
        decoder->onRead = ma_webm_read_pcm_frames_wrapper;
        decoder->onSeek = ma_webm_seek_to_pcm_frame_wrapper;
        decoder->onTell = ma_webm_get_cursor_in_pcm_frames_wrapper;

        setNextDecoder((void **)webmDecoders, (void **)&decoder, (void **)&firstWebmDecoder, &webmDecoderIndex, (uninit_func)uninitWebmDecoder);

        if (songData != NULL)
        {
                if (decoder != NULL)
                {
                        songData->duration = decoder->duration;
                }
        }

        if (currentDecoder != NULL && decoder != NULL)
        {
                if (!isEOFReached())
                        ma_data_source_set_next(currentDecoder, decoder);
        }

        return 0;
}

void getWebmFileInfo(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sampleRate, ma_channel *channelMap)
{
        ma_webm tmp;
        if (ma_webm_init_file(filename, NULL, NULL, &tmp) == MA_SUCCESS)
        {
                *sampleRate = tmp.sampleRate;
                *channels = tmp.channels;
                *format = tmp.format;
                ma_webm_uninit(&tmp, NULL);
        }
        (void)channelMap;
}

ma_webm *getFirstWebmDecoder(void)
{
        return firstWebmDecoder;
}

ma_webm *getCurrentWebmDecoder(void)
{
        if (webmDecoderIndex == -1)
                return getFirstWebmDecoder();
        else
                return webmDecoders[webmDecoderIndex];
}

void webm_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount)
{
        AudioData *pDataSource = (AudioData *)pDevice->pUserData;
        ma_uint64 framesRead = 0;
        webm_read_pcm_frames(&(pDataSource->base), pFramesOut, frameCount, &framesRead);

        if (framesRead < frameCount)
        {
                ma_webm *webm = (ma_webm *)&(pDataSource->base);
                float *output = (float *)pFramesOut;
                memset(output + framesRead * webm->channels, 0, (frameCount - framesRead) * webm->channels * sizeof(float));
        }
        (void)pFramesIn;
}
