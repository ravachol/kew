#define MA_EXPERIMENTAL__DATA_LOOPING_AND_CHAINING
#define MA_NO_ENGINE
#define MINIAUDIO_IMPLEMENTATION

/**
 * @file sound.[c]
 * @brief High-level audio playback interface.
 *
 * Provides a unified API for creating an audio device
 * and switching decoders.
 */

#include "sound.h"

#include "common/appstate.h"
#include "common/common.h"

#include "utils/file.h"

#include "sound_builtin.h"
#include "playback.h"
#ifdef USE_FAAD
#include "m4a.h"
#endif
#include "decoders.h"
#include "volume.h"
#include "audiobuffer.h"
#include "audio_file_info.h"
#include "audiotypes.h"

#include "utils/utils.h"

#include <miniaudio.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum
{
        CODEC_VORBIS,
        CODEC_WEBM
} CodecType;

typedef struct
{
        void *(*getDecoder)();

        void (*get_file_info)(
            const char *filePath,
            ma_format *pFormat,
            ma_uint32 *pChannels,
            ma_uint32 *pSampleRate,
            ma_channel *pChannelMap);

        ma_result (*getDecoderFormat)(
            ma_data_source *pDataSource,
            ma_format *pFormat,
            ma_uint32 *pChannels,
            ma_uint32 *pSampleRate,
            ma_channel *pChannelMap,
            size_t channelMapCap);

        int (*create_audio_device)(
            UserData *userData,
            ma_device *device,
            ma_context *context);

        enum AudioImplementation implType;
        bool supportsGapless;
} CodecOps;

static ma_context context;
static bool context_initialized = false;

int builtin_createAudioDevice(UserData *userData, ma_device *device, ma_context *context, ma_data_source_vtable *vtable);
int builtin_createAudioDeviceWrapper(UserData *userData, ma_device *device, ma_context *context);
int vorbis_createAudioDevice(UserData *userData, ma_device *device, ma_context *context);
int opus_createAudioDevice(UserData *userData, ma_device *device, ma_context *context);
int webm_createAudioDevice(UserData *userData, ma_device *device, ma_context *context);
#ifdef USE_FAAD
int m4a_createAudioDevice(UserData *userData, ma_device *device, ma_context *context);
#endif
bool valid_file_path(char *filePath)
{
        if (filePath == NULL || filePath[0] == '\0' || filePath[0] == '\r')
                return false;

        if (exists_file(filePath) < 0)
                return false;

        return true;
}

long long get_file_size(const char *filename)
{
        struct stat st;
        if (stat(filename, &st) == 0)
        {
                return (long long)st.st_size;
        }
        else
        {
                return -1;
        }
}

ma_result call_read_PCM_frames(ma_data_source *pDataSource, ma_format format,
                            void *pFramesOut, ma_uint64 framesRead,
                            ma_uint32 channels, ma_uint64 remainingFrames,
                            ma_uint64 *pFramesToRead)
{
        ma_result result;

        switch (format)
        {
        case ma_format_u8:
        {
                ma_uint8 *pOut = (ma_uint8 *)pFramesOut;
                result = ma_data_source_read_pcm_frames(
                    pDataSource, pOut + (framesRead * channels),
                    remainingFrames, pFramesToRead);
        }
        break;

        case ma_format_s16:
        {
                ma_int16 *pOut = (ma_int16 *)pFramesOut;
                result = ma_data_source_read_pcm_frames(
                    pDataSource, pOut + (framesRead * channels),
                    remainingFrames, pFramesToRead);
        }
        break;

        case ma_format_s24:
        {
                ma_uint8 *pOut = (ma_uint8 *)pFramesOut;
                result = ma_data_source_read_pcm_frames(
                    pDataSource, pOut + (framesRead * channels * 3),
                    remainingFrames, pFramesToRead);
        }
        break;

        case ma_format_s32:
        {
                ma_int32 *pOut = (ma_int32 *)pFramesOut;
                result = ma_data_source_read_pcm_frames(
                    pDataSource, pOut + (framesRead * channels),
                    remainingFrames, pFramesToRead);
        }
        break;

        case ma_format_f32:
        {
                float *pOut = (float *)pFramesOut;
                result = ma_data_source_read_pcm_frames(
                    pDataSource, pOut + (framesRead * channels),
                    remainingFrames, pFramesToRead);
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

int calc_avg_bit_rate(double duration, const char *filePath)
{
        long long fileSize = get_file_size(filePath); // in bytes
        int avgBitRate = 0;

        if (duration > 0.0)
                avgBitRate = (int)((fileSize * 8.0) / duration /
                                   1000.0); // use 1000 for kbps

        return avgBitRate;
}

int handle_codec(
    const char *filePath,
    CodecOps ops,
    AudioData *audio_data,
    AppState *state,
    ma_context *context)
{
        ma_uint32 sample_rate, channels, nSampleRate, nChannels;
        ma_format format, nFormat;
        ma_channel channelMap[MA_MAX_CHANNELS], nChannelMap[MA_MAX_CHANNELS];
        enum AudioImplementation current_implementation =
            get_current_implementation_type();

        ops.get_file_info(filePath, &format, &channels, &sample_rate, channelMap);

        void *decoder = ops.getDecoder();
        if (decoder != NULL && ops.getDecoderFormat)
                ops.getDecoderFormat(decoder, &nFormat, &nChannels, &nSampleRate,
                                     nChannelMap, MA_MAX_CHANNELS);

        int avgBitRate = 0;
#ifdef USE_FAAD
        k_m4adec_filetype fileType = 0;

        if (ops.implType == M4A)
        {
                get_m4a_extra_info(filePath, &avgBitRate, &fileType);
        }
#endif
        // sameFormat computation
        bool sameFormat = false;
        if (ops.supportsGapless && decoder != NULL)
        {
                sameFormat = (format == nFormat && channels == nChannels &&
                              sample_rate == nSampleRate);
#ifdef USE_FAAD
                if (ops.implType == M4A && decoder != NULL)
                {
                        sameFormat = sameFormat &&
                                     (((m4a_decoder *)decoder)->fileType == fileType) &&
                                     (((m4a_decoder *)decoder)->fileType != k_rawAAC);
                }
#endif
        }

        // Avg bitrate assignment
        if (audio_data->pUserData->currentSongData)
        {
                if (ops.implType == M4A)
                        audio_data->pUserData->currentSongData->avgBitRate = audio_data->avgBitRate = avgBitRate;
                else
                        audio_data->pUserData->currentSongData->avgBitRate =
                            audio_data->avgBitRate =
                                calc_avg_bit_rate(audio_data->pUserData->currentSongData->duration, filePath);
        }
        else
        {
                audio_data->avgBitRate = 0;
        }

        // Avg bitrate handling
        if (audio_data->pUserData->currentSongData)
                audio_data->pUserData->currentSongData->avgBitRate =
                    audio_data->avgBitRate =
                        calc_avg_bit_rate(audio_data->pUserData->currentSongData->duration, filePath);
        else
                audio_data->avgBitRate = 0;

        if (is_repeat_enabled() || !(sameFormat && current_implementation == ops.implType))
        {
                set_impl_switch_reached();

                pthread_mutex_lock(&(state->data_source_mutex));

                set_current_implementation_type(ops.implType);

                cleanup_playback_device();
                reset_all_decoders();
                reset_audio_buffer();

                audio_data->sample_rate = sample_rate;

                int result;

                if (ops.implType == BUILTIN)
                        result = builtin_createAudioDevice(audio_data->pUserData, get_device(), context,
                                                           &builtin_file_data_source_vtable);
                else
                        result = ops.create_audio_device(audio_data->pUserData, get_device(), context);

                if (result < 0)
                {
                        set_current_implementation_type(NONE);
                        set_impl_switch_not_reached();
                        set_EOF_reached();
                        pthread_mutex_unlock(&(state->data_source_mutex));
                        return -1;
                }

                pthread_mutex_unlock(&(state->data_source_mutex));
                set_impl_switch_not_reached();
        }

        return 0;
}

static void get_builtin_file_info_wrapper(
    const char *filePath,
    ma_format *pFormat,
    ma_uint32 *pChannels,
    ma_uint32 *pSampleRate,
    ma_channel *pChannelMap)
{
        (void)pChannelMap; // not used for builtin decoders
        get_file_info(filePath, pSampleRate, pChannels, pFormat);
}

static const struct
{
        const char *extension;
        CodecOps ops;
}

codec_ops_list[] = {
    {NULL, {.getDecoder = (void *(*)(void))get_current_builtin_decoder, .get_file_info = get_builtin_file_info_wrapper, .getDecoderFormat = (ma_result (*)(ma_data_source *, ma_format *, ma_uint32 *, ma_uint32 *, ma_channel *, size_t))ma_decoder_get_data_format, .create_audio_device = builtin_createAudioDeviceWrapper, .implType = BUILTIN, .supportsGapless = true}},

    {"opus", {.getDecoder = (void *(*)(void))get_current_opus_decoder, .get_file_info = get_opus_file_info, .getDecoderFormat = (ma_result (*)(ma_data_source *, ma_format *, ma_uint32 *, ma_uint32 *, ma_channel *, size_t))ma_libopus_ds_get_data_format, .create_audio_device = opus_createAudioDevice, .implType = OPUS, .supportsGapless = true}},

    {"ogg", {.getDecoder = (void *(*)(void))get_current_vorbis_decoder, .get_file_info = get_vorbis_file_info, .getDecoderFormat = (ma_result (*)(ma_data_source *, ma_format *, ma_uint32 *, ma_uint32 *, ma_channel *, size_t))ma_libvorbis_ds_get_data_format, .create_audio_device = vorbis_createAudioDevice, .implType = VORBIS, .supportsGapless = true}},

    {"webm", {.getDecoder = (void *(*)(void))get_current_webm_decoder, .get_file_info = get_webm_file_info, .getDecoderFormat = (ma_result (*)(ma_data_source *, ma_format *, ma_uint32 *, ma_uint32 *, ma_channel *, size_t))ma_webm_ds_get_data_format, .create_audio_device = webm_createAudioDevice, .implType = WEBM, .supportsGapless = false}},
#ifdef USE_FAAD
    {"m4a", {.getDecoder = (void *(*)(void))get_current_m4a_decoder, .get_file_info = get_m4a_file_info, .getDecoderFormat = (ma_result (*)(ma_data_source *, ma_format *, ma_uint32 *, ma_uint32 *, ma_channel *, size_t))m4a_decoder_ds_get_data_format, .create_audio_device = m4a_createAudioDevice, .implType = M4A, .supportsGapless = true}},
#endif
};

static const CodecOps *find_codec_ops(const char *filePath)
{
        if (has_builtin_decoder(filePath))
                return &codec_ops_list[0].ops;

        for (size_t i = 1; i < sizeof(codec_ops_list) / sizeof(codec_ops_list[0]); i++)
        {
                if (path_ends_with(filePath, codec_ops_list[i].extension))
                        return &codec_ops_list[i].ops;
        }
        return NULL;
}

static void handle_builtin_avg_bit_rate(AudioData *audio_data, SongData *songData, const char *filePath)
{
        if (path_ends_with(filePath, ".mp3") && songData)
        {
                int avgBitRate = calc_avg_bit_rate(songData->duration, filePath);
                if (avgBitRate > 320)
                        avgBitRate = 320;
                songData->avgBitRate = audio_data->avgBitRate = avgBitRate;
        }
        else
        {
                audio_data->avgBitRate = 0;
        }
}

static int prepare_next_decoder_for_codec(const char *filePath, const CodecOps *ops)
{
        if (!ops)
                return -1;

        switch (ops->implType)
        {
        case BUILTIN:
                return prepare_next_decoder(filePath); // existing builtin prep
        case OPUS:
                return prepare_next_opus_decoder(filePath);
        case VORBIS:
                return prepare_next_vorbis_decoder(filePath);
        case WEBM:
                return prepare_next_webm_decoder(audio_data.pUserData->currentSongData);
        case M4A:
#ifdef USE_FAAD
                return prepare_next_m4a_decoder(audio_data.pUserData->currentSongData);
#else
                return -1;
#endif
        default:
                return -1;
        }
}

static ma_data_source *get_first_decoder_for_codec(const CodecOps *ops)
{
        if (!ops)
                return NULL;

        switch (ops->implType)
        {
        case BUILTIN:
                return (ma_data_source *)get_first_decoder();
        case OPUS:
                return (ma_data_source *)get_first_opus_decoder();
        case VORBIS:
                return (ma_data_source *)get_first_vorbis_decoder();
        case WEBM:
                return (ma_data_source *)get_first_webm_decoder();
        case M4A:
#ifdef USE_FAAD
                return (ma_data_source *)get_first_m4a_decoder();
#else
                return NULL;
#endif
        default:
                return NULL;
        }
}

static int init_audio_data_from_codec_decoder(const CodecOps *ops, void *decoder, AudioData *audio_data)
{
        if (!ops || !decoder || !audio_data)
                return -1;

        ma_channel channelMap[MA_MAX_CHANNELS];

        switch (ops->implType)
        {
        case BUILTIN:
        {
                ma_decoder *d = (ma_decoder *)decoder;
                audio_data->format = d->outputFormat;
                audio_data->channels = d->outputChannels;
                audio_data->sample_rate = d->outputSampleRate;
                ma_data_source_get_length_in_pcm_frames((ma_data_source *)d, &audio_data->totalFrames);
                break;
        }

        case OPUS:
        {
                ma_libopus *d = (ma_libopus *)decoder;
                ma_libopus_ds_get_data_format(d, &audio_data->format, &audio_data->channels,
                                              &audio_data->sample_rate, channelMap, MA_MAX_CHANNELS);
                ma_data_source_get_length_in_pcm_frames((ma_data_source *)d, &audio_data->totalFrames);

                ((ma_data_source_base *)d)->pCurrent = d;
                d->pReadSeekTellUserData = audio_data;
                break;
        }

        case VORBIS:
        {
                ma_libvorbis *d = (ma_libvorbis *)decoder;
                ma_libvorbis_ds_get_data_format(d, &audio_data->format, &audio_data->channels,
                                                &audio_data->sample_rate, channelMap, MA_MAX_CHANNELS);
                ma_data_source_get_length_in_pcm_frames((ma_data_source *)d, &audio_data->totalFrames);

                ((ma_data_source_base *)d)->pCurrent = d;
                d->pReadSeekTellUserData = audio_data;
                break;
        }

        case WEBM:
        {
                ma_webm *d = (ma_webm *)decoder;
                ma_webm_ds_get_data_format(d, &audio_data->format, &audio_data->channels,
                                           &audio_data->sample_rate, channelMap, MA_MAX_CHANNELS);
                ma_data_source_get_length_in_pcm_frames((ma_data_source *)d, &audio_data->totalFrames);

                ((ma_data_source_base *)d)->pCurrent = d;
                d->pReadSeekTellUserData = audio_data;
                break;
        }

#ifdef USE_FAAD
        case M4A:
        {
                m4a_decoder *d = (m4a_decoder *)decoder;
                m4a_decoder_ds_get_data_format(d, &audio_data->format, &audio_data->channels,
                                               &audio_data->sample_rate, channelMap, MA_MAX_CHANNELS);
                ma_data_source_get_length_in_pcm_frames((ma_data_source *)d, &audio_data->totalFrames);

                ((ma_data_source_base *)d)->pCurrent = d;
                d->pReadSeekTellUserData = audio_data;
                break;
        }
#endif

        default:
                return -1;
        }

        return 0;
}

int init_first_datasource(UserData *userData)
{
        if (!userData)
                return MA_ERROR;

        SongData *songData = (audio_data.currentFileIndex == 0) ? userData->songdataA : userData->songdataB;
        if (!songData)
                return MA_ERROR;

        const char *filePath = songData->filePath;
        if (!filePath)
                return MA_ERROR;

        audio_data.pUserData = userData;
        audio_data.currentPCMFrame = 0;
        audio_data.restart = false;

        const CodecOps *ops = find_codec_ops(filePath);
        if (!ops)
                return MA_ERROR;

        int result = prepare_next_decoder_for_codec(filePath, ops);
        if (result < 0)
                return -1;

        void *decoder = get_first_decoder_for_codec(ops);
        if (!decoder)
                return -1;

        result = init_audio_data_from_codec_decoder(ops, decoder, &audio_data);
        if (result < 0)
                return -1;

        // BUILTIN MP3 special handling
        if (ops->implType == BUILTIN)
                handle_builtin_avg_bit_rate(&audio_data, songData, filePath);

        return MA_SUCCESS;
}
int create_device(UserData *userData, ma_device *device, ma_context *context,
                 ma_data_source_vtable *vtable, ma_device_data_proc callback)
{
        PlaybackState *ps = get_playback_state();
        ma_result result;

        result = init_first_datasource(userData);

        if (result != MA_SUCCESS)
        {
                set_error_message("Failed to initialize audio file.");
                return -1;
        }

        audio_data.base.vtable = vtable;

        result = init_playback_device(context, audio_data.format, audio_data.channels, audio_data.sample_rate,
                                    device, callback, &audio_data);

        set_volume(get_current_volume());

        ps->notifyPlaying = true;

        return 0;
}

int builtin_createAudioDevice(UserData *userData, ma_device *device,
                              ma_context *context,
                              ma_data_source_vtable *vtable)
{
        return create_device(userData, device, context, vtable, builtin_on_audio_frames);
}

int builtin_createAudioDeviceWrapper(UserData *userData,
                                     ma_device *device,
                                     ma_context *context)
{
        return builtin_createAudioDevice(userData, device, context, &builtin_file_data_source_vtable);
}

int vorbis_createAudioDevice(UserData *userData, ma_device *device,
                             ma_context *context)
{
        PlaybackState *ps = get_playback_state();
        ma_result result = init_first_datasource(userData);

        if (result != MA_SUCCESS)
        {
                set_error_message("Failed to initialize ogg vorbis file.");
                return -1;
        }

        ma_libvorbis *decoder = get_first_vorbis_decoder();

        result = init_playback_device(context, decoder->format, audio_data.channels, audio_data.sample_rate,
                                    device, vorbis_on_audio_frames, decoder);

        set_volume(get_current_volume());

        ps->notifyPlaying = true;

        return 0;
}

#ifdef USE_FAAD
int m4a_createAudioDevice(UserData *userData, ma_device *device,
                          ma_context *context)
{
        PlaybackState *ps = get_playback_state();
        ma_result result = init_first_datasource(userData);

        if (result != MA_SUCCESS)
        {
                if (!has_error_message())
                        set_error_message("M4a type not supported.");
                return -1;
        }

        m4a_decoder *decoder = get_first_m4a_decoder();

        result = init_playback_device(context, decoder->format, audio_data.channels, audio_data.sample_rate,
                                    device, m4a_on_audio_frames, decoder);

        set_volume(get_current_volume());

        ps->notifyPlaying = true;

        return 0;
}
#endif

int opus_createAudioDevice(UserData *userData, ma_device *device,
                           ma_context *context)
{
        PlaybackState *ps = get_playback_state();
        ma_result result;

        result = init_first_datasource(userData);

        if (result != MA_SUCCESS)
        {
                printf(_("\n\nFailed to initialize opus file.\n"));
                return -1;
        }

        ma_libopus *decoder = get_first_opus_decoder();

        result = init_playback_device(context, decoder->format, audio_data.channels, audio_data.sample_rate,
                                    device, opus_on_audio_frames, decoder);

        set_volume(get_current_volume());

        ps->notifyPlaying = true;

        return 0;
}

int webm_createAudioDevice(UserData *userData, ma_device *device,
                           ma_context *context)
{
        PlaybackState *ps = get_playback_state();
        ma_result result;

        result = init_first_datasource(userData);
        if (result != MA_SUCCESS)
        {
                printf(_("\n\nFailed to initialize webm file.\n"));
                return -1;
        }
        ma_webm *decoder = get_first_webm_decoder();

        result = init_playback_device(context, decoder->format, audio_data.channels, audio_data.sample_rate,
                                    device, webm_on_audio_frames, decoder);

        set_volume(get_current_volume());

        ps->notifyPlaying = true;

        return 0;
}


int switch_audio_implementation(void)
{
        AppState *state = get_app_state();

        if (audio_data.endOfListReached)
        {
                set_EOF_handled();
                set_current_implementation_type(NONE);
                return 0;
        }

        audio_data.pUserData->currentSongData = (audio_data.currentFileIndex == 0) ? audio_data.pUserData->songdataA : audio_data.pUserData->songdataB;
        if (!audio_data.pUserData->currentSongData)
        {
                set_EOF_handled();
                return 0;
        }

        char *filePath = strdup(audio_data.pUserData->currentSongData->filePath);
        if (!valid_file_path(filePath))
        {
                free(filePath);
                set_EOF_reached();
                return -1;
        }

        const CodecOps *ops = find_codec_ops(filePath);
        if (!ops)
        {
                free(filePath);
                return -1;
        }

        if (ops->implType == BUILTIN)
                handle_builtin_avg_bit_rate(&audio_data, audio_data.pUserData->currentSongData, filePath);

        int result = handle_codec(filePath, *ops, &audio_data, state, &context);
        free(filePath);

        if (result < 0)
        {
                set_current_implementation_type(NONE);
                set_impl_switch_not_reached();
                set_EOF_reached();
                return -1;
        }

        set_EOF_handled();
        return 0;
}

bool is_context_initialized(void) { return context_initialized; }

void cleanup_audio_context(void)
{
        ma_context_uninit(&context);
        context_initialized = false;
}

int create_audio_device(void)
{
        PlaybackState *ps = get_playback_state();

        if (context_initialized)
        {
                ma_context_uninit(&context);
                context_initialized = false;
        }
        ma_context_init(NULL, 0, NULL, &context);
        context_initialized = true;

        if (switch_audio_implementation() >= 0)
        {
                ps->notifySwitch = true;
        }
        else
        {
                return -1;
        }

        return 0;
}
