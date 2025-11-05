#include "decoders.h"

#include "playback.h"

#ifdef USE_FAAD
#include "m4a.h"
#endif

#include "sound/audio_file_info.h"

#define MAX_DECODERS 2

static ma_decoder *first_decoder;
static ma_decoder *current_decoder;
static ma_decoder *decoders[MAX_DECODERS];
static ma_libopus *opus_decoders[MAX_DECODERS];
static ma_libopus *first_opus_decoder;
static ma_libvorbis *vorbis_decoders[MAX_DECODERS];
static ma_libvorbis *first_vorbis_decoder;
static ma_webm *webm_decoders[MAX_DECODERS];
static ma_webm *first_webm_decoder;

#ifdef USE_FAAD
static m4a_decoder *m4a_decoders[MAX_DECODERS];
static m4a_decoder *first_m4a_decoder;
#endif

static int decoder_index = -1;
static int m4a_decoder_index = -1;
static int opus_decoder_index = -1;
static int vorbis_decoder_index = -1;
static int webm_decoder_index = -1;

void switch_specific_decoder(int *decoder_index)
{
        if (*decoder_index == -1)
                *decoder_index = 0;
        else
                *decoder_index = 1 - *decoder_index;
}

void switch_decoder(void)
{
        switch_specific_decoder(&decoder_index);
        switch_specific_decoder(&opus_decoder_index);
        switch_specific_decoder(&m4a_decoder_index);
        switch_specific_decoder(&vorbis_decoder_index);
        switch_specific_decoder(&webm_decoder_index);
}

void uninit_ma_decoder(void *decoder)
{
        ma_decoder_uninit((ma_decoder *)decoder);
}

void uninit_opus_decoder(void *decoder)
{
        ma_libopus_uninit((ma_libopus *)decoder, NULL);
}

void uninit_vorbis_decoder(void *decoder)
{
        ma_libvorbis_uninit((ma_libvorbis *)decoder, NULL);
}

void uninit_webm_decoder(void *decoder)
{
        ma_webm_uninit((ma_webm *)decoder, NULL);
}

#ifdef USE_FAAD
void uninit_m4a_decoder(void *decoder)
{
        m4a_decoder_uninit((m4a_decoder *)decoder, NULL);
}
#endif

void uninit_previous_decoder(void **decoder_array, int index, uninit_func uninit)
{
        if (index == -1) {
                return;
        }

        void *to_uninit = decoder_array[1 - index];

        if (to_uninit != NULL) {
                uninit(to_uninit);
                free(to_uninit);
                decoder_array[1 - index] = NULL;
        }
}

bool has_builtin_decoder(const char *file_path)
{
        char *extension = strrchr(file_path, '.');
        return (extension != NULL && (strcasecmp(extension, ".wav") == 0 ||
                                      strcasecmp(extension, ".flac") == 0 ||
                                      strcasecmp(extension, ".mp3") == 0));
}

void clear_decoder_chain()
{
        ma_data_source_set_next(current_decoder, NULL);
}

ma_decoder *get_first_decoder(void)
{
        return first_decoder;
}

ma_decoder *get_current_builtin_decoder(void)
{
        if (decoder_index == -1)
                return get_first_decoder();
        else
                return decoders[decoder_index];
}

#ifdef USE_FAAD
m4a_decoder *get_first_m4a_decoder(void)
{
        return first_m4a_decoder;
}

m4a_decoder *get_current_m4a_decoder(void)
{
        if (m4a_decoder_index == -1)
                return get_first_m4a_decoder();
        else
                return m4a_decoders[m4a_decoder_index];
}
#endif

void reset_decoders(void **decoder_array, void **first_decoder, int array_size,
                    int *decoder_index, uninit_func uninit)
{
        *decoder_index = -1;

        if (*first_decoder != NULL) {
                uninit(*first_decoder);
                free(*first_decoder);
                *first_decoder = NULL;
        }

        for (int i = 0; i < array_size; i++) {
                if (decoder_array[i] != NULL) {
                        uninit(decoder_array[i]);
                        free(decoder_array[i]);
                        decoder_array[i] = NULL;
                }
        }
}

void reset_all_decoders(void)
{
        reset_decoders((void **)decoders, (void **)&first_decoder, MAX_DECODERS,
                       &decoder_index, uninit_ma_decoder);
        reset_decoders((void **)vorbis_decoders, (void **)&first_vorbis_decoder,
                       MAX_DECODERS, &vorbis_decoder_index, uninit_vorbis_decoder);
        reset_decoders((void **)opus_decoders, (void **)&first_opus_decoder,
                       MAX_DECODERS, &opus_decoder_index, uninit_opus_decoder);
        reset_decoders((void **)webm_decoders, (void **)&first_webm_decoder,
                       MAX_DECODERS, &webm_decoder_index, uninit_webm_decoder);
#ifdef USE_FAAD
        reset_decoders((void **)m4a_decoders, (void **)&first_m4a_decoder,
                       MAX_DECODERS, &m4a_decoder_index, uninit_m4a_decoder);
#endif
}

void set_next_decoder(void **decoder_array, void **decoder, void **first_decoder,
                      int *decoder_index, uninit_func uninit)
{
        if (*decoder_index == -1 && *first_decoder == NULL) {
                *first_decoder = *decoder;
        } else if (*decoder_index == -1) // Array hasn't been used yet
        {
                if (decoder_array[0] != NULL) {
                        uninit(decoder_array[0]);
                        free(decoder_array[0]);
                        decoder_array[0] = NULL;
                }

                decoder_array[0] = *decoder;
        } else {
                int next_index = 1 - *decoder_index;
                if (decoder_array[next_index] != NULL) {
                        uninit(decoder_array[next_index]);
                        free(decoder_array[next_index]);
                        decoder_array[next_index] = NULL;
                }

                decoder_array[next_index] = *decoder;
        }
}

MA_API ma_result ma_libopus_read_pcm_frames_wrapper(void *pDecoder,
                                                    void *p_frames_out,
                                                    size_t frame_count,
                                                    size_t *p_frames_read)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_libopus_read_pcm_frames((ma_libopus *)dec->pUserData,
                                          p_frames_out, frame_count,
                                          (ma_uint64 *)p_frames_read);
}

MA_API ma_result ma_libopus_seek_to_pcm_frame_wrapper(void *pDecoder,
                                                      long long int frame_index,
                                                      ma_seek_origin origin)
{
        (void)origin;
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_libopus_seek_to_pcm_frame((ma_libopus *)dec->pUserData,
                                            frame_index);
}

MA_API ma_result ma_libopus_get_cursor_in_pcm_frames_wrapper(
    void *pDecoder, long long int *p_cursor)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_libopus_get_cursor_in_pcm_frames((ma_libopus *)dec->pUserData,
                                                   (ma_uint64 *)p_cursor);
}

MA_API ma_result ma_libvorbis_read_pcm_frames_wrapper(void *pDecoder,
                                                      void *p_frames_out,
                                                      size_t frame_count,
                                                      size_t *p_frames_read)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_libvorbis_read_pcm_frames((ma_libvorbis *)dec->pUserData,
                                            p_frames_out, frame_count,
                                            (ma_uint64 *)p_frames_read);
}

MA_API ma_result ma_libvorbis_seek_to_pcm_frame_wrapper(
    void *pDecoder, long long int frame_index, ma_seek_origin origin)
{
        (void)origin;
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_libvorbis_seek_to_pcm_frame((ma_libvorbis *)dec->pUserData,
                                              frame_index);
}

MA_API ma_result ma_libvorbis_get_cursor_in_pcm_frames_wrapper(
    void *pDecoder, long long int *p_cursor)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_libvorbis_get_cursor_in_pcm_frames(
            (ma_libvorbis *)dec->pUserData, (ma_uint64 *)p_cursor);
}

MA_API ma_result ma_webm_read_pcm_frames_wrapper(void *pDecoder,
                                                 void *p_frames_out,
                                                 size_t frame_count,
                                                 size_t *p_frames_read)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_webm_read_pcm_frames((ma_webm *)dec->pUserData, p_frames_out,
                                       frame_count, (ma_uint64 *)p_frames_read);
}

MA_API ma_result ma_webm_seek_to_pcm_frame_wrapper(void *pDecoder,
                                                   long long int frame_index,
                                                   ma_seek_origin origin)
{
        (void)origin;
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_webm_seek_to_pcm_frame((ma_webm *)dec->pUserData, frame_index);
}

MA_API ma_result
ma_webm_get_cursor_in_pcm_frames_wrapper(void *pDecoder, long long int *p_cursor)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return ma_webm_get_cursor_in_pcm_frames((ma_webm *)dec->pUserData,
                                                (ma_uint64 *)p_cursor);
}
#ifdef USE_FAAD
MA_API ma_result m4a_read_pcm_frames_wrapper(void *pDecoder, void *p_frames_out,
                                             size_t frame_count,
                                             size_t *p_frames_read)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return m4a_decoder_read_pcm_frames((m4a_decoder *)dec->pUserData,
                                           p_frames_out, frame_count,
                                           (ma_uint64 *)p_frames_read);
}

MA_API ma_result m4a_seek_to_pcm_frame_wrapper(void *pDecoder,
                                               long long int frame_index,
                                               ma_seek_origin origin)
{
        (void)origin;
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return m4a_decoder_seek_to_pcm_frame((m4a_decoder *)dec->pUserData,
                                             frame_index);
}

MA_API ma_result m4a_get_cursor_in_pcm_frames_wrapper(void *pDecoder,
                                                      long long int *p_cursor)
{
        ma_decoder *dec = (ma_decoder *)pDecoder;
        return m4a_decoder_get_cursor_in_pcm_frames(
            (m4a_decoder *)dec->pUserData, (ma_uint64 *)p_cursor);
}

int prepare_next_m4a_decoder(SongData *song_data)
{
        m4a_decoder *current_decoder;

        if (song_data == NULL)
                return -1;

        char *filepath = song_data->file_path;

        if (m4a_decoder_index == -1) {
                current_decoder = get_first_m4a_decoder();
        } else {
                current_decoder = m4a_decoders[m4a_decoder_index];
        }

        ma_uint32 sample_rate;
        ma_uint32 channels;
        ma_format format;
        ma_channel channel_map[MA_MAX_CHANNELS];
        m4a_decoder_get_data_format(current_decoder, &format, &channels,
                                    &sample_rate, channel_map, MA_MAX_CHANNELS);

        uninit_previous_decoder((void **)m4a_decoders, m4a_decoder_index,
                                (uninit_func)uninit_m4a_decoder);

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
        bool sameFormat = (current_decoder == NULL ||
                           (format == nformat && channels == nchannels &&
                            sample_rate == nsampleRate &&
                            current_decoder->file_type == decoder->file_type &&
                            current_decoder->file_type != k_rawAAC));

        if (!sameFormat) {
                m4a_decoder_uninit(decoder, NULL);
                free(decoder);
                return 0;
        }

        m4a_decoder *first = get_first_m4a_decoder();
        if (first != NULL) {
                decoder->pReadSeekTellUserData =
                    (AudioData *)first->pReadSeekTellUserData;
        }

        decoder->format = nformat;
        decoder->onRead = m4a_read_pcm_frames_wrapper;
        decoder->onSeek = m4a_seek_to_pcm_frame_wrapper;
        decoder->onTell = m4a_get_cursor_in_pcm_frames_wrapper;
        decoder->cursor = 0;

        set_next_decoder((void **)m4a_decoders, (void **)&decoder,
                         (void **)&first_m4a_decoder, &m4a_decoder_index,
                         (uninit_func)uninit_m4a_decoder);

        if (song_data != NULL) {
                if (decoder != NULL && decoder->file_type == k_rawAAC) {
                        song_data->duration = decoder->duration;
                }
        }

        if (current_decoder != NULL && decoder != NULL &&
            decoder->file_type != k_rawAAC) {
                if (!pb_is_EOF_reached())
                        ma_data_source_set_next(current_decoder, decoder);
        }
        return 0;
}
#endif

ma_libvorbis *get_first_vorbis_decoder(void)
{
        return first_vorbis_decoder;
}

ma_libopus *get_first_opus_decoder(void)
{
        return first_opus_decoder;
}

ma_libvorbis *get_current_vorbis_decoder(void)
{
        if (vorbis_decoder_index == -1)
                return get_first_vorbis_decoder();
        else
                return vorbis_decoders[vorbis_decoder_index];
}

ma_libopus *get_current_opus_decoder(void)
{
        if (opus_decoder_index == -1)
                return get_first_opus_decoder();
        else
                return opus_decoders[opus_decoder_index];
}

int prepare_next_vorbis_decoder(const char *filepath)
{
        ma_libvorbis *current_decoder;

        if (vorbis_decoder_index == -1) {
                current_decoder = get_first_vorbis_decoder();
        } else {
                current_decoder = vorbis_decoders[vorbis_decoder_index];
        }

        ma_uint32 sample_rate;
        ma_uint32 channels;
        ma_format format;
        ma_channel channel_map[MA_MAX_CHANNELS];
        ma_libvorbis_get_data_format(current_decoder, &format, &channels,
                                     &sample_rate, channel_map, MA_MAX_CHANNELS);

        uninit_previous_decoder((void **)vorbis_decoders, vorbis_decoder_index,
                                (uninit_func)uninit_vorbis_decoder);

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
        bool sameFormat = (current_decoder == NULL ||
                           (format == nformat && channels == nchannels &&
                            sample_rate == nsampleRate));

        if (!sameFormat) {
                ma_libvorbis_uninit(decoder, NULL);
                free(decoder);
                return 0;
        }

        ma_libvorbis *first = get_first_vorbis_decoder();
        if (first != NULL) {
                decoder->pReadSeekTellUserData =
                    (AudioData *)first->pReadSeekTellUserData;
        }

        decoder->format = nformat;
        decoder->onRead = ma_libvorbis_read_pcm_frames_wrapper;
        decoder->onSeek = ma_libvorbis_seek_to_pcm_frame_wrapper;
        decoder->onTell = ma_libvorbis_get_cursor_in_pcm_frames_wrapper;

        set_next_decoder((void **)vorbis_decoders, (void **)&decoder,
                         (void **)&first_vorbis_decoder, &vorbis_decoder_index,
                         (uninit_func)uninit_vorbis_decoder);

        if (current_decoder != NULL && decoder != NULL) {
                if (!pb_is_EOF_reached())
                        ma_data_source_set_next(current_decoder, decoder);
        }

        return 0;
}

int prepare_next_decoder(const char *filepath)
{
        ma_decoder *current_decoder;

        if (decoder_index == -1) {
                current_decoder = get_first_decoder();
        } else {
                current_decoder = decoders[decoder_index];
        }

        ma_uint32 sample_rate;
        ma_uint32 channels;
        ma_format format;
        get_file_info(filepath, &sample_rate, &channels, &format);

        bool sameFormat = (current_decoder == NULL ||
                           (format == current_decoder->outputFormat &&
                            channels == current_decoder->outputChannels &&
                            sample_rate == current_decoder->outputSampleRate));

        if (!sameFormat) {
                return 0;
        }

        uninit_previous_decoder((void **)decoders, decoder_index,
                                (uninit_func)uninit_ma_decoder);

        ma_decoder *decoder = (ma_decoder *)malloc(sizeof(ma_decoder));
        ma_result result = ma_decoder_init_file(filepath, NULL, decoder);

        if (result != MA_SUCCESS) {
                free(decoder);
                return -1;
        }
        set_next_decoder((void **)decoders, (void **)&decoder,
                         (void **)&first_decoder, &decoder_index,
                         (uninit_func)uninit_ma_decoder);

        if (current_decoder != NULL && decoder != NULL) {
                if (!pb_is_EOF_reached())
                        ma_data_source_set_next(current_decoder, decoder);
        }
        return 0;
}

int prepare_next_opus_decoder(const char *filepath)
{
        ma_libopus *current_decoder;

        if (opus_decoder_index == -1) {
                current_decoder = get_first_opus_decoder();
        } else {
                current_decoder = opus_decoders[opus_decoder_index];
        }

        ma_uint32 sample_rate;
        ma_uint32 channels;
        ma_format format;
        ma_channel channel_map[MA_MAX_CHANNELS];
        ma_libopus_get_data_format(current_decoder, &format, &channels,
                                   &sample_rate, channel_map, MA_MAX_CHANNELS);

        uninit_previous_decoder((void **)opus_decoders, opus_decoder_index,
                                (uninit_func)uninit_opus_decoder);

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
        bool sameFormat = (current_decoder == NULL ||
                           (format == nformat && channels == nchannels &&
                            sample_rate == nsampleRate));

        if (!sameFormat) {
                ma_libopus_uninit(decoder, NULL);
                free(decoder);
                return 0;
        }

        if (first_opus_decoder != NULL) {
                decoder->pReadSeekTellUserData =
                    (AudioData *)first_opus_decoder->pReadSeekTellUserData;
        }

        decoder->format = nformat;
        decoder->onRead = ma_libopus_read_pcm_frames_wrapper;
        decoder->onSeek = ma_libopus_seek_to_pcm_frame_wrapper;
        decoder->onTell = ma_libopus_get_cursor_in_pcm_frames_wrapper;

        set_next_decoder((void **)opus_decoders, (void **)&decoder,
                         (void **)&first_opus_decoder, &opus_decoder_index,
                         (uninit_func)uninit_opus_decoder);

        if (current_decoder != NULL && decoder != NULL) {
                if (!pb_is_EOF_reached())
                        ma_data_source_set_next(current_decoder, decoder);
        }
        return 0;
}

int prepare_next_webm_decoder(SongData *song_data)
{
        ma_webm *current_decoder;

        if (song_data == NULL)
                return -1;

        char *filepath = song_data->file_path;

        if (webm_decoder_index == -1) {
                current_decoder = get_first_webm_decoder();
        } else {
                current_decoder = webm_decoders[webm_decoder_index];
        }

        ma_uint32 sample_rate;
        ma_uint32 channels;
        ma_format format;
        ma_channel channel_map[MA_MAX_CHANNELS];
        ma_webm_get_data_format(current_decoder, &format, &channels, &sample_rate,
                                channel_map, MA_MAX_CHANNELS);

        uninit_previous_decoder((void **)webm_decoders, webm_decoder_index,
                                (uninit_func)uninit_webm_decoder);

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

        bool sameFormat = (current_decoder == NULL);

        // Gapless playback disabled for webm
        // bool sameFormat = (current_decoder == NULL || (format == nformat &&
        //                                              channels == nchannels &&
        //                                              sample_rate ==
        //                                              nsampleRate));

        if (!sameFormat) {
                ma_webm_uninit(decoder, NULL);
                free(decoder);
                return 0;
        }

        if (first_webm_decoder != NULL) {
                decoder->pReadSeekTellUserData =
                    (AudioData *)first_webm_decoder->pReadSeekTellUserData;
        }

        decoder->format = nformat;
        decoder->onRead = ma_webm_read_pcm_frames_wrapper;
        decoder->onSeek = ma_webm_seek_to_pcm_frame_wrapper;
        decoder->onTell = ma_webm_get_cursor_in_pcm_frames_wrapper;

        set_next_decoder((void **)webm_decoders, (void **)&decoder,
                         (void **)&first_webm_decoder, &webm_decoder_index,
                         (uninit_func)uninit_webm_decoder);

        if (song_data != NULL) {
                if (decoder != NULL) {
                        song_data->duration = decoder->duration;
                }
        }

        if (current_decoder != NULL && decoder != NULL) {
                if (!pb_is_EOF_reached())
                        ma_data_source_set_next(current_decoder, decoder);
        }

        return 0;
}

ma_webm *get_first_webm_decoder(void)
{
        return first_webm_decoder;
}

ma_webm *get_current_webm_decoder(void)
{
        if (webm_decoder_index == -1)
                return get_first_webm_decoder();
        else
                return webm_decoders[webm_decoder_index];
}
