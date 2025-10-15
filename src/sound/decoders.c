#include "decoders.h"

#include "playback.h"
#include "m4a.h"
#include "sound/audiofileinfo.h"

#define MAX_DECODERS 2

static ma_decoder *firstDecoder;
static ma_decoder *currentDecoder;
static ma_decoder *decoders[MAX_DECODERS];
static ma_libopus *opusDecoders[MAX_DECODERS];
static ma_libopus *firstOpusDecoder;
static ma_libvorbis *vorbisDecoders[MAX_DECODERS];
static ma_libvorbis *firstVorbisDecoder;
static ma_webm *webmDecoders[MAX_DECODERS];
static ma_webm *firstWebmDecoder;

#ifdef USE_FAAD
static m4a_decoder *m4aDecoders[MAX_DECODERS];
static m4a_decoder *firstM4aDecoder;
#endif

static int decoderIndex = -1;
static int m4aDecoderIndex = -1;
static int opusDecoderIndex = -1;
static int vorbisDecoderIndex = -1;
static int webmDecoderIndex = -1;

void switchSpecificDecoder(int *decoderIndex)
{
        if (*decoderIndex == -1)
                *decoderIndex = 0;
        else
                *decoderIndex = 1 - *decoderIndex;
}

void switchDecoder(void)
{
        switchSpecificDecoder(&decoderIndex);
        switchSpecificDecoder(&opusDecoderIndex);
        switchSpecificDecoder(&m4aDecoderIndex);
        switchSpecificDecoder(&vorbisDecoderIndex);
        switchSpecificDecoder(&webmDecoderIndex);
}

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

bool hasBuiltinDecoder(const char *filePath)
{
        char *extension = strrchr(filePath, '.');
        return (extension != NULL && (strcasecmp(extension, ".wav") == 0 ||
                                      strcasecmp(extension, ".flac") == 0 ||
                                      strcasecmp(extension, ".mp3") == 0));
}


void clearDecoderChain()
{
        ma_data_source_set_next(currentDecoder, NULL);
}

ma_decoder *getFirstDecoder(void) { return firstDecoder; }

ma_decoder *getCurrentBuiltinDecoder(void)
{
        if (decoderIndex == -1)
                return getFirstDecoder();
        else
                return decoders[decoderIndex];
}

#ifdef USE_FAAD
m4a_decoder *getFirstM4aDecoder(void) { return firstM4aDecoder; }
#endif

m4a_decoder *getCurrentM4aDecoder(void)
{
        if (m4aDecoderIndex == -1)
                return getFirstM4aDecoder();
        else
                return m4aDecoders[m4aDecoderIndex];
}

void resetDecoders(void **decoderArray, void **firstDecoder, int arraySize,
                   int *decoderIndex, uninit_func uninit)
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

void resetAllDecoders(void)
{
        resetDecoders((void **)decoders, (void **)&firstDecoder, MAX_DECODERS,
                      &decoderIndex, uninitMaDecoder);
        resetDecoders((void **)vorbisDecoders, (void **)&firstVorbisDecoder,
                      MAX_DECODERS, &vorbisDecoderIndex, uninitVorbisDecoder);
        resetDecoders((void **)opusDecoders, (void **)&firstOpusDecoder,
                      MAX_DECODERS, &opusDecoderIndex, uninitOpusDecoder);
        resetDecoders((void **)webmDecoders, (void **)&firstWebmDecoder,
                      MAX_DECODERS, &webmDecoderIndex, uninitWebmDecoder);
#ifdef USE_FAAD
        resetDecoders((void **)m4aDecoders, (void **)&firstM4aDecoder,
                      MAX_DECODERS, &m4aDecoderIndex, uninitM4aDecoder);
#endif
}

void setNextDecoder(void **decoderArray, void **decoder, void **firstDecoder,
                    int *decoderIndex, uninit_func uninit)
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

MA_API ma_result ma_libopus_read_pcm_frames_wrapper(void *pDecoder,
                                                    void *pFramesOut,
                                                    size_t frameCount,
                                                    size_t *pFramesRead)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_libopus_read_pcm_frames((ma_libopus *)dec->pUserData,
                                          pFramesOut, frameCount,
                                          (ma_uint64 *)pFramesRead);
}

MA_API ma_result ma_libopus_seek_to_pcm_frame_wrapper(void *pDecoder,
                                                      long long int frameIndex,
                                                      ma_seek_origin origin)
{
        (void)origin;
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_libopus_seek_to_pcm_frame((ma_libopus *)dec->pUserData,
                                            frameIndex);
}

MA_API ma_result ma_libopus_get_cursor_in_pcm_frames_wrapper(
    void *pDecoder, long long int *pCursor)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_libopus_get_cursor_in_pcm_frames((ma_libopus *)dec->pUserData,
                                                   (ma_uint64 *)pCursor);
}

MA_API ma_result ma_libvorbis_read_pcm_frames_wrapper(void *pDecoder,
                                                      void *pFramesOut,
                                                      size_t frameCount,
                                                      size_t *pFramesRead)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_libvorbis_read_pcm_frames((ma_libvorbis *)dec->pUserData,
                                            pFramesOut, frameCount,
                                            (ma_uint64 *)pFramesRead);
}

MA_API ma_result ma_libvorbis_seek_to_pcm_frame_wrapper(
    void *pDecoder, long long int frameIndex, ma_seek_origin origin)
{
        (void)origin;
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_libvorbis_seek_to_pcm_frame((ma_libvorbis *)dec->pUserData,
                                              frameIndex);
}

MA_API ma_result ma_libvorbis_get_cursor_in_pcm_frames_wrapper(
    void *pDecoder, long long int *pCursor)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_libvorbis_get_cursor_in_pcm_frames(
            (ma_libvorbis *)dec->pUserData, (ma_uint64 *)pCursor);
}

MA_API ma_result ma_webm_read_pcm_frames_wrapper(void *pDecoder,
                                                 void *pFramesOut,
                                                 size_t frameCount,
                                                 size_t *pFramesRead)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_webm_read_pcm_frames((ma_webm *)dec->pUserData, pFramesOut,
                                       frameCount, (ma_uint64 *)pFramesRead);
}

MA_API ma_result ma_webm_seek_to_pcm_frame_wrapper(void *pDecoder,
                                                   long long int frameIndex,
                                                   ma_seek_origin origin)
{
        (void)origin;
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_webm_seek_to_pcm_frame((ma_webm *)dec->pUserData, frameIndex);
}

MA_API ma_result
ma_webm_get_cursor_in_pcm_frames_wrapper(void *pDecoder, long long int *pCursor)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_webm_get_cursor_in_pcm_frames((ma_webm *)dec->pUserData,
                                                (ma_uint64 *)pCursor);
}

MA_API ma_result m4a_read_pcm_frames_wrapper(void *pDecoder, void *pFramesOut,
                                             size_t frameCount,
                                             size_t *pFramesRead)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return m4a_decoder_read_pcm_frames((m4a_decoder *)dec->pUserData,
                                           pFramesOut, frameCount,
                                           (ma_uint64 *)pFramesRead);
}

MA_API ma_result m4a_seek_to_pcm_frame_wrapper(void *pDecoder,
                                               long long int frameIndex,
                                               ma_seek_origin origin)
{
        (void)origin;
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return m4a_decoder_seek_to_pcm_frame((m4a_decoder *)dec->pUserData,
                                             frameIndex);
}

MA_API ma_result m4a_get_cursor_in_pcm_frames_wrapper(void *pDecoder,
                                                      long long int *pCursor)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return m4a_decoder_get_cursor_in_pcm_frames(
            (m4a_decoder *)dec->pUserData, (ma_uint64 *)pCursor);
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
        m4a_decoder_get_data_format(currentDecoder, &format, &channels,
                                    &sampleRate, channelMap, MA_MAX_CHANNELS);

        uninitPreviousDecoder((void **)m4aDecoders, m4aDecoderIndex,
                              (uninit_func)uninitM4aDecoder);

        m4a_decoder *decoder = (m4a_decoder *)malloc(sizeof(m4a_decoder));
        ma_result result = m4a_decoder_init_file(filepath, NULL, NULL, decoder);

        if (result != MA_SUCCESS)
                return -1;

        ma_format nformat;
        ma_uint32 nchannels;
        ma_uint32 nsampleRate;
        ma_channel nchannelMap[MA_MAX_CHANNELS];

        m4a_decoder_get_data_format(decoder, &nformat, &nchannels, &nsampleRate,
                                    nchannelMap, MA_MAX_CHANNELS);
        bool sameFormat = (currentDecoder == NULL ||
                           (format == nformat && channels == nchannels &&
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
                decoder->pReadSeekTellUserData =
                    (AudioData *)first->pReadSeekTellUserData;
        }

        decoder->format = nformat;
        decoder->onRead = m4a_read_pcm_frames_wrapper;
        decoder->onSeek = m4a_seek_to_pcm_frame_wrapper;
        decoder->onTell = m4a_get_cursor_in_pcm_frames_wrapper;
        decoder->cursor = 0;

        setNextDecoder((void **)m4aDecoders, (void **)&decoder,
                       (void **)&firstM4aDecoder, &m4aDecoderIndex,
                       (uninit_func)uninitM4aDecoder);

        if (songData != NULL)
        {
                if (decoder != NULL && decoder->fileType == k_rawAAC)
                {
                        songData->duration = decoder->duration;
                }
        }

        if (currentDecoder != NULL && decoder != NULL &&
            decoder->fileType != k_rawAAC)
        {
                if (!isEOFReached())
                        ma_data_source_set_next(currentDecoder, decoder);
        }
        return 0;
}

ma_libvorbis *getFirstVorbisDecoder(void) { return firstVorbisDecoder; }

ma_libopus *getFirstOpusDecoder(void) { return firstOpusDecoder; }

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

int prepareNextVorbisDecoder(const char *filepath)
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
        ma_libvorbis_get_data_format(currentDecoder, &format, &channels,
                                     &sampleRate, channelMap, MA_MAX_CHANNELS);

        uninitPreviousDecoder((void **)vorbisDecoders, vorbisDecoderIndex,
                              (uninit_func)uninitVorbisDecoder);

        ma_libvorbis *decoder = (ma_libvorbis *)malloc(sizeof(ma_libvorbis));
        ma_result result =
            ma_libvorbis_init_file(filepath, NULL, NULL, decoder);

        if (result != MA_SUCCESS)
                return -1;

        ma_format nformat;
        ma_uint32 nchannels;
        ma_uint32 nsampleRate;
        ma_channel nchannelMap[MA_MAX_CHANNELS];

        ma_libvorbis_get_data_format(decoder, &nformat, &nchannels,
                                     &nsampleRate, nchannelMap,
                                     MA_MAX_CHANNELS);
        bool sameFormat = (currentDecoder == NULL ||
                           (format == nformat && channels == nchannels &&
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
                decoder->pReadSeekTellUserData =
                    (AudioData *)first->pReadSeekTellUserData;
        }

        decoder->format = nformat;
        decoder->onRead = ma_libvorbis_read_pcm_frames_wrapper;
        decoder->onSeek = ma_libvorbis_seek_to_pcm_frame_wrapper;
        decoder->onTell = ma_libvorbis_get_cursor_in_pcm_frames_wrapper;

        setNextDecoder((void **)vorbisDecoders, (void **)&decoder,
                       (void **)&firstVorbisDecoder, &vorbisDecoderIndex,
                       (uninit_func)uninitVorbisDecoder);

        if (currentDecoder != NULL && decoder != NULL)
        {
                if (!isEOFReached())
                        ma_data_source_set_next(currentDecoder, decoder);
        }

        return 0;
}

int prepareNextDecoder(const char *filepath)
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

        bool sameFormat = (currentDecoder == NULL ||
                           (format == currentDecoder->outputFormat &&
                            channels == currentDecoder->outputChannels &&
                            sampleRate == currentDecoder->outputSampleRate));

        if (!sameFormat)
        {
                return 0;
        }

        uninitPreviousDecoder((void **)decoders, decoderIndex,
                              (uninit_func)uninitMaDecoder);

        ma_decoder *decoder = (ma_decoder *)malloc(sizeof(ma_decoder));
        ma_result result = ma_decoder_init_file(filepath, NULL, decoder);

        if (result != MA_SUCCESS)
        {
                free(decoder);
                return -1;
        }
        setNextDecoder((void **)decoders, (void **)&decoder,
                       (void **)&firstDecoder, &decoderIndex,
                       (uninit_func)uninitMaDecoder);

        if (currentDecoder != NULL && decoder != NULL)
        {
                if (!isEOFReached())
                        ma_data_source_set_next(currentDecoder, decoder);
        }
        return 0;
}

int prepareNextOpusDecoder(const char *filepath)
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
        ma_libopus_get_data_format(currentDecoder, &format, &channels,
                                   &sampleRate, channelMap, MA_MAX_CHANNELS);

        uninitPreviousDecoder((void **)opusDecoders, opusDecoderIndex,
                              (uninit_func)uninitOpusDecoder);

        ma_libopus *decoder = (ma_libopus *)malloc(sizeof(ma_libopus));
        ma_result result = ma_libopus_init_file(filepath, NULL, NULL, decoder);

        if (result != MA_SUCCESS)
                return -1;

        ma_format nformat;
        ma_uint32 nchannels;
        ma_uint32 nsampleRate;
        ma_channel nchannelMap[MA_MAX_CHANNELS];

        ma_libopus_get_data_format(decoder, &nformat, &nchannels, &nsampleRate,
                                   nchannelMap, MA_MAX_CHANNELS);
        bool sameFormat = (currentDecoder == NULL ||
                           (format == nformat && channels == nchannels &&
                            sampleRate == nsampleRate));

        if (!sameFormat)
        {
                ma_libopus_uninit(decoder, NULL);
                free(decoder);
                return 0;
        }

        if (firstOpusDecoder != NULL)
        {
                decoder->pReadSeekTellUserData =
                    (AudioData *)firstOpusDecoder->pReadSeekTellUserData;
        }

        decoder->format = nformat;
        decoder->onRead = ma_libopus_read_pcm_frames_wrapper;
        decoder->onSeek = ma_libopus_seek_to_pcm_frame_wrapper;
        decoder->onTell = ma_libopus_get_cursor_in_pcm_frames_wrapper;

        setNextDecoder((void **)opusDecoders, (void **)&decoder,
                       (void **)&firstOpusDecoder, &opusDecoderIndex,
                       (uninit_func)uninitOpusDecoder);

        if (currentDecoder != NULL && decoder != NULL)
        {
                if (!isEOFReached())
                        ma_data_source_set_next(currentDecoder, decoder);
        }
        return 0;
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
        ma_webm_get_data_format(currentDecoder, &format, &channels, &sampleRate,
                                channelMap, MA_MAX_CHANNELS);

        uninitPreviousDecoder((void **)webmDecoders, webmDecoderIndex,
                              (uninit_func)uninitWebmDecoder);

        ma_webm *decoder = (ma_webm *)malloc(sizeof(ma_webm));
        ma_result result = ma_webm_init_file(filepath, NULL, NULL, decoder);

        if (result != MA_SUCCESS)
                return -1;

        ma_format nformat;
        ma_uint32 nchannels;
        ma_uint32 nsampleRate;
        ma_channel nchannelMap[MA_MAX_CHANNELS];

        ma_webm_get_data_format(decoder, &nformat, &nchannels, &nsampleRate,
                                nchannelMap, MA_MAX_CHANNELS);

        bool sameFormat = (currentDecoder == NULL);

        // Gapless playback disabled for webm
        // bool sameFormat = (currentDecoder == NULL || (format == nformat &&
        //                                              channels == nchannels &&
        //                                              sampleRate ==
        //                                              nsampleRate));

        if (!sameFormat)
        {
                ma_webm_uninit(decoder, NULL);
                free(decoder);
                return 0;
        }

        if (firstWebmDecoder != NULL)
        {
                decoder->pReadSeekTellUserData =
                    (AudioData *)firstWebmDecoder->pReadSeekTellUserData;
        }

        decoder->format = nformat;
        decoder->onRead = ma_webm_read_pcm_frames_wrapper;
        decoder->onSeek = ma_webm_seek_to_pcm_frame_wrapper;
        decoder->onTell = ma_webm_get_cursor_in_pcm_frames_wrapper;

        setNextDecoder((void **)webmDecoders, (void **)&decoder,
                       (void **)&firstWebmDecoder, &webmDecoderIndex,
                       (uninit_func)uninitWebmDecoder);

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

ma_webm *getFirstWebmDecoder(void) { return firstWebmDecoder; }

ma_webm *getCurrentWebmDecoder(void)
{
        if (webmDecoderIndex == -1)
                return getFirstWebmDecoder();
        else
                return webmDecoders[webmDecoderIndex];
}
