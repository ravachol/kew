/**
 * @file decoders.c
 * @brief Decoders.
 *
 * Decoder related functions.
 */

#include "decoders.h"

#include "playback.h"

#include "audiotypes.h"
#include "utils/utils.h"

#ifdef USE_FAAD
#include "m4a.h"
#endif

#include "sound/audio_file_info.h"
#include "webm.h"

#define MAX_DECODERS 2

/* Decoder chain data */

// The first one is the start of the chain and is stored separately.
// You need it to access the chain.
// The others alternate between pos 0 and 1 in the array.
// They must all belong to the same decoder type.

static void *first_decoder;
static void *decoders[MAX_DECODERS];
static int decoder_index = -1;
static atomic_int decoder_decoder_type = NONE;

/* Init decoder wrappers */

static ma_result init_ma_decoder_wrapper(
    const char *filepath,
    ma_decoding_backend_config *config,
    void *decoder)
{
        ma_decoder_config decConfig =
            ma_decoder_config_init(
                config->preferredFormat,
                0,
                0);

        ma_result result =
            ma_decoder_init_file(
                filepath,
                &decConfig,
                (ma_decoder *)decoder);

        return (int)result;
}

static ma_result init_vorbis_decoder(const char *pFilePath, const ma_decoding_backend_config *p_config,
                                     ma_libvorbis *decoder)
{
        return ma_libvorbis_init_file(pFilePath, p_config, NULL, decoder);
}

static ma_result init_opus_decoder(const char *pFilePath, const ma_decoding_backend_config *p_config,
                                   ma_libopus *decoder)
{
        return ma_libopus_init_file(pFilePath, p_config, NULL, decoder);
}

#ifdef USE_FAAD
static ma_result init_m4a_decoder(const char *pFilePath, const ma_decoding_backend_config *p_config,
                                  ma_m4a *pM4a)
{
        return m4a_decoder_init_file(pFilePath, p_config, NULL, pM4a);
}
#endif

static ma_result init_webm_decoder(const char *pFilePath, const ma_decoding_backend_config *p_config,
                                   ma_webm *decoder)
{
        return ma_webm_init_file(pFilePath, p_config, NULL, decoder);
}

static void get_builtin_file_info_wrapper(
    const char *file_path,
    ma_format *p_format,
    ma_uint32 *p_channels,
    ma_uint32 *p_sample_rate,
    ma_channel *p_channel_map)
{
        (void)p_channel_map; // not used for builtin decoders
        get_file_info(file_path, p_sample_rate, p_channels, p_format);
}

/* Get data format wrappers */

static ma_result opus_get_data_format_wrapper(ma_data_source *pDataSource, ma_format *pFormat, ma_uint32 *pChannels, ma_uint32 *pSampleRate, ma_channel *pChannelMap, size_t channelMapCap)
{
        return ma_libopus_get_data_format((ma_libopus *)pDataSource, pFormat, pChannels, pSampleRate, pChannelMap, channelMapCap);
}

static ma_result vorbis_get_data_format_wrapper(ma_data_source *pDataSource, ma_format *pFormat, ma_uint32 *pChannels, ma_uint32 *pSampleRate, ma_channel *pChannelMap, size_t channelMapCap)
{
        return ma_libvorbis_get_data_format((ma_libvorbis *)pDataSource, pFormat, pChannels, pSampleRate, pChannelMap, channelMapCap);
}

static ma_result webm_get_data_format_wrapper(ma_data_source *p_data_source, ma_format *p_format, ma_uint32 *p_channels, ma_uint32 *p_sample_rate, ma_channel *p_channel_map, size_t channel_map_cap)
{
        return ma_webm_get_data_format((ma_webm *)p_data_source, p_format, p_channels, p_sample_rate, p_channel_map, channel_map_cap);
}

/* Get cursor wrappers */

static ma_result ma_decoder_get_cursor_in_pcm_frames_wrapper(
    void *pDecoder, long long *p_cursor)
{
        return ma_data_source_get_cursor_in_pcm_frames((ma_decoder *)pDecoder,
                                                       (ma_uint64 *)p_cursor);
}

static ma_result ma_libvorbis_get_cursor_in_pcm_frames_wrapper(
    void *pDecoder, long long *p_cursor)
{
        return ma_libvorbis_get_cursor_in_pcm_frames((ma_libvorbis *)pDecoder,
                                                     (ma_uint64 *)p_cursor);
}

static ma_result ma_libopus_get_cursor_in_pcm_frames_wrapper(
    void *pDecoder, long long *p_cursor)
{
        return ma_libopus_get_cursor_in_pcm_frames((ma_libopus *)pDecoder,
                                                   (ma_uint64 *)p_cursor);
}

#ifdef USE_FAAD
static ma_result m4a_get_cursor_in_pcm_frames_wrapper(void *pDecoder,
                                                      long long int *p_cursor)
{
        return m4a_decoder_get_cursor_in_pcm_frames((ma_m4a *)pDecoder,
                                                    (ma_uint64 *)p_cursor);
}
#endif

static ma_result ma_webm_get_cursor_in_pcm_frames_wrapper(void *pDecoder, long long int *p_cursor)
{
        return ma_webm_get_cursor_in_pcm_frames((ma_webm *)pDecoder,
                                                (ma_uint64 *)p_cursor);
}

/* Read wrappers */

static ma_result ma_libvorbis_read_pcm_frames_wrapper(void *pDecoder,
                                                      void *p_frames_out,
                                                      size_t frame_count,
                                                      size_t *p_frames_read)
{
        return ma_libvorbis_read_pcm_frames((ma_libvorbis *)pDecoder,
                                            p_frames_out, frame_count,
                                            (ma_uint64 *)p_frames_read);
}

static ma_result ma_libopus_read_pcm_frames_wrapper(void *pDecoder,
                                                    void *p_frames_out,
                                                    size_t frame_count,
                                                    size_t *p_frames_read)
{
        return ma_libopus_read_pcm_frames((ma_libopus *)pDecoder,
                                          p_frames_out, frame_count,
                                          (ma_uint64 *)p_frames_read);
}

#ifdef USE_FAAD
static ma_result m4a_read_pcm_frames_wrapper(void *pDecoder, void *p_frames_out,
                                             size_t frame_count,
                                             size_t *p_frames_read)
{
        return m4a_decoder_read_pcm_frames((ma_m4a *)pDecoder,
                                           p_frames_out, frame_count,
                                           (ma_uint64 *)p_frames_read);
}
#endif

static ma_result ma_webm_read_pcm_frames_wrapper(void *pDecoder,
                                                 void *p_frames_out,
                                                 size_t frame_count,
                                                 size_t *p_frames_read)
{
        return ma_webm_read_pcm_frames((ma_webm *)pDecoder, p_frames_out,
                                       frame_count, (ma_uint64 *)p_frames_read);
}

/* Seek wrappers */

static ma_result ma_decoder_seek_to_pcm_frame_wrapper(
    void *pDecoder, long long int frame_index, ma_seek_origin origin)
{
        (void)origin;
        return ma_decoder_seek_to_pcm_frame((ma_decoder *)pDecoder, frame_index);
}

static ma_result ma_libvorbis_seek_to_pcm_frame_wrapper(
    void *pDecoder, long long int frame_index, ma_seek_origin origin)
{
        (void)origin;
        return ma_libvorbis_seek_to_pcm_frame((ma_libvorbis *)pDecoder,
                                              frame_index);
}

static ma_result ma_libopus_seek_to_pcm_frame_wrapper(void *pDecoder,
                                                      long long int frame_index,
                                                      ma_seek_origin origin)
{
        (void)origin;
        return ma_libopus_seek_to_pcm_frame((ma_libopus *)pDecoder,
                                            frame_index);
}

#ifdef USE_FAAD
static ma_result m4a_seek_to_pcm_frame_wrapper(void *pDecoder,
                                               long long int frame_index,
                                               ma_seek_origin origin)
{
        (void)origin;
        return m4a_decoder_seek_to_pcm_frame((ma_m4a *)pDecoder,
                                             frame_index);
}
#endif

static ma_result ma_webm_seek_to_pcm_frame_wrapper(void *pDecoder,
                                                   long long int frame_index,
                                                   ma_seek_origin origin)
{
        (void)origin;
        return ma_webm_seek_to_pcm_frame((ma_webm *)pDecoder, frame_index);
}

/* Uninit wrappers */

static void uninit_ma_decoder(void *decoder)
{
        ma_decoder_uninit((ma_decoder *)decoder);
}

static void uninit_opus_decoder(void *decoder)
{
        ma_libopus_uninit((ma_libopus *)decoder, NULL);
}

static void uninit_vorbis_decoder(void *decoder)
{
        ma_libvorbis_uninit((ma_libvorbis *)decoder, NULL);
}

#ifdef USE_FAAD
static void uninit_m4a_decoder(void *decoder)
{
        m4a_decoder_uninit((ma_m4a *)decoder, NULL);
}
#endif

static void uninit_webm_decoder(void *decoder)
{
        ma_webm_uninit((ma_webm *)decoder, NULL);
}

/* Setup helpers */

static void setup_ma_decoder(void *decoder, void *firstDecoder)
{
        (void)decoder;
        (void)firstDecoder;
        /* nothing to do */
}

static void setup_libvorbis(void *decoder, void *firstDecoder)
{
        ma_libvorbis *d = (ma_libvorbis *)decoder;
        d->onRead = ma_libvorbis_read_pcm_frames_wrapper;
        d->onSeek = ma_libvorbis_seek_to_pcm_frame_wrapper;
        d->onTell = ma_libvorbis_get_cursor_in_pcm_frames_wrapper;

        if (firstDecoder)
                d->pReadSeekTellUserData = ((ma_libvorbis *)firstDecoder)->pReadSeekTellUserData;
}

static void setup_libopus(void *decoder, void *firstDecoder)
{
        ma_libopus *d = (ma_libopus *)decoder;
        d->onRead = ma_libopus_read_pcm_frames_wrapper;
        d->onSeek = ma_libopus_seek_to_pcm_frame_wrapper;
        d->onTell = ma_libopus_get_cursor_in_pcm_frames_wrapper;

        if (firstDecoder != NULL) {
                d->pReadSeekTellUserData = ((ma_libopus *)firstDecoder)->pReadSeekTellUserData;
        }
}

static void setup_webm(void *decoder, void *firstDecoder)
{
        ma_webm *d = (ma_webm *)decoder;
        d->onRead = ma_webm_read_pcm_frames_wrapper;
        d->onSeek = ma_webm_seek_to_pcm_frame_wrapper;
        d->onTell = ma_webm_get_cursor_in_pcm_frames_wrapper;

        if (firstDecoder)
                d->pReadSeekTellUserData = ((ma_webm *)firstDecoder)->pReadSeekTellUserData;
}

#ifdef USE_FAAD
static void setup_m4a(void *decoder, void *firstDecoder)
{
        ma_m4a *d = (ma_m4a *)decoder;
        d->onRead = m4a_read_pcm_frames_wrapper;
        d->onSeek = m4a_seek_to_pcm_frame_wrapper;
        d->onTell = m4a_get_cursor_in_pcm_frames_wrapper;
        d->cursor = 0;

        if (firstDecoder)
                d->pReadSeekTellUserData = ((ma_webm *)firstDecoder)->pReadSeekTellUserData;
}
#endif

/* codec_op_list - used to enable generic functions for decode handling */

// clang-format off

static const CodecEntry codec_ops_list[] = {
    {NULL, {
        .get_file_info    = get_builtin_file_info_wrapper,
        .get_decoder_format = (decoder_format_func)ma_decoder_get_data_format,
        .seek_to_pcm_frame  = ma_decoder_seek_to_pcm_frame_wrapper,
        .get_cursor       = ma_decoder_get_cursor_in_pcm_frames_wrapper,
        .decoder_type         = BUILTIN,
        .supportsGapless  = true,
        .setup_decoder    = setup_ma_decoder,
        .decoderSize      = sizeof(ma_decoder),
        .init             = (init_func)init_ma_decoder_wrapper,
        .uninit           = uninit_ma_decoder
    }},

    {"opus", {
        .get_file_info    = get_opus_file_info,
        .get_decoder_format = (decoder_format_func)opus_get_data_format_wrapper,
        .seek_to_pcm_frame  = ma_libopus_seek_to_pcm_frame_wrapper,
        .get_cursor       = ma_libopus_get_cursor_in_pcm_frames_wrapper,
        .decoder_type         = OPUS,
        .supportsGapless  = true,
        .setup_decoder    = setup_libopus,
        .decoderSize      = sizeof(ma_libopus),
        .init             = (init_func)init_opus_decoder,
        .uninit           = (uninit_func)uninit_opus_decoder
    }},

    {"ogg", {
        .get_file_info    = get_vorbis_file_info,
        .get_decoder_format = (decoder_format_func)vorbis_get_data_format_wrapper,
        .seek_to_pcm_frame  = ma_libvorbis_seek_to_pcm_frame_wrapper,
        .get_cursor       = ma_libvorbis_get_cursor_in_pcm_frames_wrapper,
        .decoder_type         = VORBIS,
        .supportsGapless  = true,
        .setup_decoder    = setup_libvorbis,
        .decoderSize      = sizeof(ma_libvorbis),
        .init             = (init_func)init_vorbis_decoder,
        .uninit           = (uninit_func)uninit_vorbis_decoder
    }},

    {"webm", {
        .get_file_info    = get_webm_file_info,
        .get_decoder_format = (decoder_format_func)webm_get_data_format_wrapper,
        .seek_to_pcm_frame  = ma_webm_seek_to_pcm_frame_wrapper,
        .get_cursor       = ma_webm_get_cursor_in_pcm_frames_wrapper,
        .decoder_type         = WEBM,
        .supportsGapless  = false,
        .setup_decoder    = setup_webm,
        .decoderSize      = sizeof(ma_webm),
        .init             = (init_func)init_webm_decoder,
        .uninit           = (uninit_func)uninit_webm_decoder
    }},
#ifdef USE_FAAD
    {"m4a", {
        .get_file_info    = get_m4a_file_info,
        .get_decoder_format = (decoder_format_func)m4a_decoder_ds_get_data_format,
        .seek_to_pcm_frame  = m4a_seek_to_pcm_frame_wrapper,
        .get_cursor       = m4a_get_cursor_in_pcm_frames_wrapper,
        .decoder_type         = M4A,
        .supportsGapless  = true,
        .setup_decoder    = setup_m4a,
        .decoderSize      = sizeof(m4a_decoder),
        .init             = (init_func)init_m4a_decoder,
        .uninit           = (uninit_func)uninit_m4a_decoder
    }},
    {"aac", {
        .get_file_info    = get_m4a_file_info,
        .get_decoder_format = (decoder_format_func)m4a_decoder_ds_get_data_format,
        .seek_to_pcm_frame  = m4a_seek_to_pcm_frame_wrapper,
        .get_cursor       = m4a_get_cursor_in_pcm_frames_wrapper,
        .decoder_type         = M4A,
        .supportsGapless  = true,
        .setup_decoder    = setup_m4a,
        .decoderSize      = sizeof(m4a_decoder),
        .init             = (init_func)init_m4a_decoder,
        .uninit           = (uninit_func)uninit_m4a_decoder
    }},
#endif
};

// clang-format on

/* CodecOps finder functions */

const CodecOps *get_codec_ops(enum decoder_type_t type)
{
        for (size_t i = 0; i < sizeof(codec_ops_list) / sizeof(codec_ops_list[0]); i++) {
                if (codec_ops_list[i].ops.decoder_type == type)
                        return &codec_ops_list[i].ops;
        }
        return NULL;
}

const CodecOps *find_codec_ops(const char *file_path)
{
        if (is_decoder_native(file_path))
                return &codec_ops_list[0].ops;

        for (size_t i = 1; i < sizeof(codec_ops_list) / sizeof(codec_ops_list[0]); i++) {
                if (path_ends_with(file_path, codec_ops_list[i].extension))
                        return &codec_ops_list[i].ops;
        }
        return NULL;
}

/* Decoder getters */

void *get_first_decoder(void)
{
        return first_decoder;
}

void *get_current_decoder(void)
{
        if (decoder_index == -1)
                return get_first_decoder();
        else
                return decoders[decoder_index];
}

enum decoder_type_t get_current_decoder_decoder_type(void)
{
        return atomic_load(&decoder_decoder_type);
}

int can_decoder_seek(void *decoder)
{
#ifdef USE_FAAD
        if (get_current_decoder_decoder_type() == M4A) {
                ma_m4a *dec = (ma_m4a *)decoder;
                if (dec != NULL && dec->file_type == k_rawAAC)
                        return 0;
        }
#endif

        if (get_current_decoder_decoder_type() == WEBM) {
                return 0;
        }

        return 1; // All the others can seek
}

/* Decoder index switch */

void switch_decoder_index(void)
{
        if (decoder_index == -1)
                decoder_index = 0;
        else
                decoder_index = 1 - decoder_index;
}

/* Decoder uninit previous */

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

/* Is decoder native to miniaudio? (wav/mp3/flac) */

bool is_decoder_native(const char *file_path)
{
        const char *extension = strrchr(file_path, '.');
        return (extension != NULL && (strcasecmp(extension, ".wav") == 0 ||
                                      strcasecmp(extension, ".aiff") == 0 ||
                                      strcasecmp(extension, ".flac") == 0 ||
                                      strcasecmp(extension, ".mp3") == 0));
}

/* Clear decoder chain*/

void clear_decoder_chain(void)
{
        void *current =
            (decoder_index == -1)
                ? first_decoder
                : decoders[decoder_index];

        ma_data_source_set_next(current, NULL);
}

/* Reset decoders */

void reset_decoders(void)
{
        clear_decoder_chain();

        int cur_implType = get_current_decoder_decoder_type();

        if (cur_implType == NONE)
                return;

        const CodecOps *ops = get_codec_ops(cur_implType);
        set_current_decoder_decoder_type(NONE);
        decoder_index = -1;

        if (first_decoder != NULL) {
                if (ops) {
                        ops->uninit(first_decoder);
                        free(first_decoder);
                        first_decoder = NULL;
                }
        }

        for (int i = 0; i < MAX_DECODERS; i++) {
                if (decoders[i] != NULL) {
                        if (ops) {
                                ops->uninit(decoders[i]);
                                free(decoders[i]);
                                decoders[i] = NULL;
                        }
                }
        }
}

/* Set current decoder decoder type */

void set_current_decoder_decoder_type(enum decoder_type_t new_decoder_type)
{
        atomic_store(&decoder_decoder_type, new_decoder_type);
}

/* Set next decoder */

void set_next_decoder(void *decoder, const enum decoder_type_t new_decoder_type)
{
        enum decoder_type_t cur_decoder_type = atomic_load(&decoder_decoder_type);

        if (cur_decoder_type != new_decoder_type && cur_decoder_type != NONE) // All decoders must be the same type
                return;

        const CodecOps *cur_ops = get_codec_ops(cur_decoder_type);
        set_current_decoder_decoder_type(new_decoder_type);

        if (decoder_index == -1 && first_decoder == NULL) {
                first_decoder = decoder;
        } else if (decoder_index == -1) // Array hasn't been used yet
        {
                if (decoders[0] != NULL) {
                        if (cur_ops) {
                                cur_ops->uninit(decoders[0]);
                                free(decoders[0]);
                                decoders[0] = NULL;
                        }
                }

                decoders[0] = decoder;
        } else {
                int next_index = 1 - decoder_index;
                if (decoders[next_index] != NULL) {
                        if (cur_ops) {
                                cur_ops->uninit(decoders[next_index]);
                                free(decoders[next_index]);
                                decoders[next_index] = NULL;
                        }
                }

                decoders[next_index] = decoder;
        }
}

/* Prepare next decoder */

int prepare_next_decoder(const char *filepath, SongData *song, const CodecOps *ops)
{
        void *current =
            (decoder_index == -1)
                ? first_decoder
                : decoders[decoder_index];

        ma_format format = 0;
        ma_uint32 channels = 0;
        ma_uint32 sampleRate = 0;
        ma_channel channel_map[MA_MAX_CHANNELS];

        enum decoder_type_t cur_implType = atomic_load(&decoder_decoder_type);

        bool same_impl_type = (cur_implType == ops->decoder_type);

        if (current) {
                const CodecOps *cur_ops = get_codec_ops(cur_implType);
                if (cur_ops) {
                        cur_ops->get_decoder_format(current, &format, &channels, &sampleRate, channel_map, MA_MAX_CHANNELS);
                        uninit_previous_decoder(decoders, decoder_index, (uninit_func)cur_ops->uninit);
                }
        }

        void *decoder = malloc(ops->decoderSize);
        ma_decoding_backend_config config = {0};
        config.preferredFormat = ma_format_f32;
        config.seekPointCount = 0;

        if (ops->init(filepath, &config, decoder) != MA_SUCCESS) {
                free(decoder);
                return -1;
        }

        ma_format nformat;
        ma_uint32 nchannels;
        ma_uint32 nsampleRate;
        ma_channel nchannelMap[MA_MAX_CHANNELS];
        ops->get_decoder_format(decoder, &nformat, &nchannels, &nsampleRate, nchannelMap, MA_MAX_CHANNELS);

        bool sameFormat = (current == NULL || !ops->supportsGapless ||
                           (ops->decoder_type == cur_implType &&
                            format == nformat &&
                            channels == nchannels &&
                            sampleRate == nsampleRate));

#ifdef USE_FAAD
        if (ops->decoder_type == M4A && current) {
                sameFormat = sameFormat && (((ma_m4a *)current)->file_type == ((ma_m4a *)decoder)->file_type &&
                                            ((ma_m4a *)current)->file_type != k_rawAAC);
        }
#endif

        if (!sameFormat) {
                ops->uninit(decoder);
                free(decoder);
                return 0;
        }

        if (ops->setup_decoder)
                ops->setup_decoder(decoder, first_decoder);

        if (ops->decoder_type == WEBM)
                song->duration = ((ma_webm *)decoder)->duration;

        set_next_decoder(decoder, ops->decoder_type);

        if (same_impl_type && current && decoder && !pb_is_EOF_reached())
#ifdef USE_FAAD
                if (ops->decoder_type != M4A || (current && ((ma_m4a *)current)->file_type != k_rawAAC))
#endif
                        ma_data_source_set_next(current, decoder);

        return 0;
}
