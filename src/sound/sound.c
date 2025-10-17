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

#include "soundbuiltin.h"
#include "playback.h"
#ifdef USE_FAAD
#include "m4a.h"
#endif
#include "decoders.h"
#include "volume.h"
#include "audiobuffer.h"
#include "audiofileinfo.h"
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

        void (*getFileInfo)(
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

        int (*createAudioDevice)(
            UserData *userData,
            ma_device *device,
            ma_context *context);

        enum AudioImplementation implType;
        bool supportsGapless;
} CodecOps;

UserData userData;

static ma_context context;
static bool contextInitialized = false;

int builtin_createAudioDevice(UserData *userData, ma_device *device, ma_context *context, ma_data_source_vtable *vtable);
int builtin_createAudioDeviceWrapper(UserData *userData, ma_device *device, ma_context *context);
int vorbis_createAudioDevice(UserData *userData, ma_device *device, ma_context *context);
int opus_createAudioDevice(UserData *userData, ma_device *device, ma_context *context);
int webm_createAudioDevice(UserData *userData, ma_device *device, ma_context *context);
#ifdef USE_FAAD
int m4a_createAudioDevice(UserData *userData, ma_device *device, ma_context *context);
#endif

UserData *getUserData(void) { return &userData; }

bool validFilePath(char *filePath)
{
        if (filePath == NULL || filePath[0] == '\0' || filePath[0] == '\r')
                return false;

        if (existsFile(filePath) < 0)
                return false;

        return true;
}

long long getFileSize(const char *filename)
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

ma_result callReadPCMFrames(ma_data_source *pDataSource, ma_format format,
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

int calcAvgBitRate(double duration, const char *filePath)
{
        long long fileSize = getFileSize(filePath); // in bytes
        int avgBitRate = 0;

        if (duration > 0.0)
                avgBitRate = (int)((fileSize * 8.0) / duration /
                                   1000.0); // use 1000 for kbps

        return avgBitRate;
}

int handleCodec(
    const char *filePath,
    CodecOps ops,
    AudioData *audioData,
    UserData userData,
    AppState *state,
    ma_context *context)
{
        ma_uint32 sampleRate, channels, nSampleRate, nChannels;
        ma_format format, nFormat;
        ma_channel channelMap[MA_MAX_CHANNELS], nChannelMap[MA_MAX_CHANNELS];
        enum AudioImplementation currentImplementation =
            getCurrentImplementationType();

        ops.getFileInfo(filePath, &format, &channels, &sampleRate, channelMap);

        void *decoder = ops.getDecoder();
        if (decoder != NULL && ops.getDecoderFormat)
                ops.getDecoderFormat(decoder, &nFormat, &nChannels, &nSampleRate,
                                     nChannelMap, MA_MAX_CHANNELS);

        int avgBitRate = 0;
#ifdef USE_FAAD
        k_m4adec_filetype fileType = 0;

        if (ops.implType == M4A)
        {
                getM4aExtraInfo(filePath, &avgBitRate, &fileType);
        }
#endif
        // sameFormat computation
        bool sameFormat = false;
        if (ops.supportsGapless && decoder != NULL)
        {
                sameFormat = (format == nFormat && channels == nChannels &&
                              sampleRate == nSampleRate);
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
        if (userData.currentSongData)
        {
                if (ops.implType == M4A)
                        userData.currentSongData->avgBitRate = audioData->avgBitRate = avgBitRate;
                else
                        userData.currentSongData->avgBitRate =
                            audioData->avgBitRate =
                                calcAvgBitRate(userData.currentSongData->duration, filePath);
        }
        else
        {
                audioData->avgBitRate = 0;
        }

        // Avg bitrate handling
        if (userData.currentSongData)
                userData.currentSongData->avgBitRate =
                    audioData->avgBitRate =
                        calcAvgBitRate(userData.currentSongData->duration, filePath);
        else
                audioData->avgBitRate = 0;

        if (isRepeatEnabled() || !(sameFormat && currentImplementation == ops.implType))
        {
                setImplSwitchReached();

                pthread_mutex_lock(&(state->dataSourceMutex));

                setCurrentImplementationType(ops.implType);

                cleanupPlaybackDevice();
                resetAllDecoders();
                resetAudioBuffer();

                audioData->sampleRate = sampleRate;

                int result;

                if (ops.implType == BUILTIN)
                        result = builtin_createAudioDevice(&userData, getDevice(), context,
                                                           &builtin_file_data_source_vtable);
                else
                        result = ops.createAudioDevice(&userData, getDevice(), context);

                if (result < 0)
                {
                        setCurrentImplementationType(NONE);
                        setImplSwitchNotReached();
                        setEofReached();
                        pthread_mutex_unlock(&(state->dataSourceMutex));
                        return -1;
                }

                pthread_mutex_unlock(&(state->dataSourceMutex));
                setImplSwitchNotReached();
        }

        return 0;
}

static void getBuiltinFileInfoWrapper(
    const char *filePath,
    ma_format *pFormat,
    ma_uint32 *pChannels,
    ma_uint32 *pSampleRate,
    ma_channel *pChannelMap)
{
        (void)pChannelMap; // not used for builtin decoders
        getFileInfo(filePath, pSampleRate, pChannels, pFormat);
}

static const struct
{
        const char *extension;
        CodecOps ops;
}

codecOpsList[] = {
    {NULL, {.getDecoder = (void *(*)(void))getCurrentBuiltinDecoder, .getFileInfo = getBuiltinFileInfoWrapper, .getDecoderFormat = (ma_result (*)(ma_data_source *, ma_format *, ma_uint32 *, ma_uint32 *, ma_channel *, size_t))ma_decoder_get_data_format, .createAudioDevice = builtin_createAudioDeviceWrapper, .implType = BUILTIN, .supportsGapless = true}},

    {"opus", {.getDecoder = (void *(*)(void))getCurrentOpusDecoder, .getFileInfo = getOpusFileInfo, .getDecoderFormat = (ma_result (*)(ma_data_source *, ma_format *, ma_uint32 *, ma_uint32 *, ma_channel *, size_t))ma_libopus_ds_get_data_format, .createAudioDevice = opus_createAudioDevice, .implType = OPUS, .supportsGapless = true}},

    {"ogg", {.getDecoder = (void *(*)(void))getCurrentVorbisDecoder, .getFileInfo = getVorbisFileInfo, .getDecoderFormat = (ma_result (*)(ma_data_source *, ma_format *, ma_uint32 *, ma_uint32 *, ma_channel *, size_t))ma_libvorbis_ds_get_data_format, .createAudioDevice = vorbis_createAudioDevice, .implType = VORBIS, .supportsGapless = true}},

    {"webm", {.getDecoder = (void *(*)(void))getCurrentWebmDecoder, .getFileInfo = getWebmFileInfo, .getDecoderFormat = (ma_result (*)(ma_data_source *, ma_format *, ma_uint32 *, ma_uint32 *, ma_channel *, size_t))ma_webm_ds_get_data_format, .createAudioDevice = webm_createAudioDevice, .implType = WEBM, .supportsGapless = false}},
#ifdef USE_FAAD
    {"m4a", {.getDecoder = (void *(*)(void))getCurrentM4aDecoder, .getFileInfo = getM4aFileInfo, .getDecoderFormat = (ma_result (*)(ma_data_source *, ma_format *, ma_uint32 *, ma_uint32 *, ma_channel *, size_t))m4a_decoder_ds_get_data_format, .createAudioDevice = m4a_createAudioDevice, .implType = M4A, .supportsGapless = true}},
#endif
};

static const CodecOps *findCodecOps(const char *filePath)
{
        if (hasBuiltinDecoder(filePath))
                return &codecOpsList[0].ops;

        for (size_t i = 1; i < sizeof(codecOpsList) / sizeof(codecOpsList[0]); i++)
        {
                if (pathEndsWith(filePath, codecOpsList[i].extension))
                        return &codecOpsList[i].ops;
        }
        return NULL;
}

static void handleBuiltinAvgBitRate(AudioData *audioData, SongData *songData, const char *filePath)
{
        if (pathEndsWith(filePath, ".mp3") && songData)
        {
                int avgBitRate = calcAvgBitRate(songData->duration, filePath);
                if (avgBitRate > 320)
                        avgBitRate = 320;
                songData->avgBitRate = audioData->avgBitRate = avgBitRate;
        }
        else
        {
                audioData->avgBitRate = 0;
        }
}

static int prepareNextDecoderForCodec(const char *filePath, const CodecOps *ops)
{
        if (!ops)
                return -1;

        switch (ops->implType)
        {
        case BUILTIN:
                return prepareNextDecoder(filePath); // existing builtin prep
        case OPUS:
                return prepareNextOpusDecoder(filePath);
        case VORBIS:
                return prepareNextVorbisDecoder(filePath);
        case WEBM:
                return prepareNextWebmDecoder(userData.currentSongData);
        case M4A:
#ifdef USE_FAAD
                return prepareNextM4aDecoder(userData.currentSongData);
#else
                return -1;
#endif
        default:
                return -1;
        }
}

static ma_data_source *getFirstDecoderForCodec(const CodecOps *ops)
{
        if (!ops)
                return NULL;

        switch (ops->implType)
        {
        case BUILTIN:
                return (ma_data_source *)getFirstDecoder();
        case OPUS:
                return (ma_data_source *)getFirstOpusDecoder();
        case VORBIS:
                return (ma_data_source *)getFirstVorbisDecoder();
        case WEBM:
                return (ma_data_source *)getFirstWebmDecoder();
        case M4A:
#ifdef USE_FAAD
                return (ma_data_source *)getFirstM4aDecoder();
#else
                return NULL;
#endif
        default:
                return NULL;
        }
}

static int initAudioDataFromCodecDecoder(const CodecOps *ops, void *decoder, AudioData *audioData)
{
        if (!ops || !decoder || !audioData)
                return -1;

        ma_channel channelMap[MA_MAX_CHANNELS];

        switch (ops->implType)
        {
        case BUILTIN:
        {
                ma_decoder *d = (ma_decoder *)decoder;
                audioData->format = d->outputFormat;
                audioData->channels = d->outputChannels;
                audioData->sampleRate = d->outputSampleRate;
                ma_data_source_get_length_in_pcm_frames((ma_data_source *)d, &audioData->totalFrames);
                break;
        }

        case OPUS:
        {
                ma_libopus *d = (ma_libopus *)decoder;
                ma_libopus_ds_get_data_format(d, &audioData->format, &audioData->channels,
                                              &audioData->sampleRate, channelMap, MA_MAX_CHANNELS);
                ma_data_source_get_length_in_pcm_frames((ma_data_source *)d, &audioData->totalFrames);

                ((ma_data_source_base *)d)->pCurrent = d;
                d->pReadSeekTellUserData = audioData;
                break;
        }

        case VORBIS:
        {
                ma_libvorbis *d = (ma_libvorbis *)decoder;
                ma_libvorbis_ds_get_data_format(d, &audioData->format, &audioData->channels,
                                                &audioData->sampleRate, channelMap, MA_MAX_CHANNELS);
                ma_data_source_get_length_in_pcm_frames((ma_data_source *)d, &audioData->totalFrames);

                ((ma_data_source_base *)d)->pCurrent = d;
                d->pReadSeekTellUserData = audioData;
                break;
        }

        case WEBM:
        {
                ma_webm *d = (ma_webm *)decoder;
                ma_webm_ds_get_data_format(d, &audioData->format, &audioData->channels,
                                           &audioData->sampleRate, channelMap, MA_MAX_CHANNELS);
                ma_data_source_get_length_in_pcm_frames((ma_data_source *)d, &audioData->totalFrames);

                ((ma_data_source_base *)d)->pCurrent = d;
                d->pReadSeekTellUserData = audioData;
                break;
        }

#ifdef USE_FAAD
        case M4A:
        {
                m4a_decoder *d = (m4a_decoder *)decoder;
                m4a_decoder_ds_get_data_format(d, &audioData->format, &audioData->channels,
                                               &audioData->sampleRate, channelMap, MA_MAX_CHANNELS);
                ma_data_source_get_length_in_pcm_frames((ma_data_source *)d, &audioData->totalFrames);

                ((ma_data_source_base *)d)->pCurrent = d;
                d->pReadSeekTellUserData = audioData;
                break;
        }
#endif

        default:
                return -1;
        }

        return 0;
}

int initFirstDatasource(UserData *userData)
{
        if (!userData)
                return MA_ERROR;

        AudioData *audioData = getAudioData();
        SongData *songData = (audioData->currentFileIndex == 0) ? userData->songdataA : userData->songdataB;
        if (!songData)
                return MA_ERROR;

        const char *filePath = songData->filePath;
        if (!filePath)
                return MA_ERROR;

        audioData->pUserData = userData;
        audioData->currentPCMFrame = 0;
        audioData->restart = false;

        const CodecOps *ops = findCodecOps(filePath);
        if (!ops)
                return MA_ERROR;

        int result = prepareNextDecoderForCodec(filePath, ops);
        if (result < 0)
                return -1;

        void *decoder = getFirstDecoderForCodec(ops);
        if (!decoder)
                return -1;

        result = initAudioDataFromCodecDecoder(ops, decoder, audioData);
        if (result < 0)
                return -1;

        // BUILTIN MP3 special handling
        if (ops->implType == BUILTIN)
                handleBuiltinAvgBitRate(audioData, songData, filePath);

        return MA_SUCCESS;
}
int createDevice(UserData *userData, ma_device *device, ma_context *context,
                 ma_data_source_vtable *vtable, ma_device_data_proc callback)
{
        PlaybackState *ps = getPlaybackState();
        ma_result result;

        result = initFirstDatasource(userData);

        if (result != MA_SUCCESS)
        {
                setErrorMessage("Failed to initialize audio file.");
                return -1;
        }

        AudioData *audioData = getAudioData();
        audioData->base.vtable = vtable;

        result = initPlaybackDevice(context, audioData->format, audioData->channels, audioData->sampleRate,
                                    device, callback, audioData);

        setVolume(getCurrentVolume());

        ps->notifyPlaying = true;

        return 0;
}

int builtin_createAudioDevice(UserData *userData, ma_device *device,
                              ma_context *context,
                              ma_data_source_vtable *vtable)
{
        return createDevice(userData, device, context, vtable, builtin_on_audio_frames);
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
        PlaybackState *ps = getPlaybackState();
        ma_result result = initFirstDatasource(userData);

        if (result != MA_SUCCESS)
        {
                setErrorMessage("Failed to initialize ogg vorbis file.");
                return -1;
        }

        ma_libvorbis *decoder = getFirstVorbisDecoder();
        AudioData *audioData = getAudioData();

        result = initPlaybackDevice(context, decoder->format, audioData->channels, audioData->sampleRate,
                                    device, vorbis_on_audio_frames, decoder);

        setVolume(getCurrentVolume());

        ps->notifyPlaying = true;

        return 0;
}

#ifdef USE_FAAD
int m4a_createAudioDevice(UserData *userData, ma_device *device,
                          ma_context *context)
{
        PlaybackState *ps = getPlaybackState();
        ma_result result = initFirstDatasource(userData);

        if (result != MA_SUCCESS)
        {
                if (!hasErrorMessage())
                        setErrorMessage("M4a type not supported.");
                return -1;
        }

        m4a_decoder *decoder = getFirstM4aDecoder();
        AudioData *audioData = getAudioData();

        result = initPlaybackDevice(context, decoder->format, audioData->channels, audioData->sampleRate,
                                    device, m4a_on_audio_frames, decoder);

        setVolume(getCurrentVolume());

        ps->notifyPlaying = true;

        return 0;
}
#endif

int opus_createAudioDevice(UserData *userData, ma_device *device,
                           ma_context *context)
{
        PlaybackState *ps = getPlaybackState();
        ma_result result;

        result = initFirstDatasource(userData);

        if (result != MA_SUCCESS)
        {
                printf("\n\nFailed to initialize opus file.\n");
                return -1;
        }

        ma_libopus *decoder = getFirstOpusDecoder();
        AudioData *audioData = getAudioData();

        result = initPlaybackDevice(context, decoder->format, audioData->channels, audioData->sampleRate,
                                    device, opus_on_audio_frames, decoder);

        setVolume(getCurrentVolume());

        ps->notifyPlaying = true;

        return 0;
}

int webm_createAudioDevice(UserData *userData, ma_device *device,
                           ma_context *context)
{
        PlaybackState *ps = getPlaybackState();
        ma_result result;

        result = initFirstDatasource(userData);
        if (result != MA_SUCCESS)
        {
                printf("\n\nFailed to initialize webm file.\n");
                return -1;
        }
        ma_webm *decoder = getFirstWebmDecoder();
        AudioData *audioData = getAudioData();

        result = initPlaybackDevice(context, decoder->format, audioData->channels, audioData->sampleRate,
                                    device, webm_on_audio_frames, decoder);

        setVolume(getCurrentVolume());

        ps->notifyPlaying = true;

        return 0;
}


int switchAudioImplementation(void)
{
        AppState *state = getAppState();
        AudioData *audioData = getAudioData();

        if (audioData->endOfListReached)
        {
                setEofHandled();
                setCurrentImplementationType(NONE);
                return 0;
        }

        userData.currentSongData = (audioData->currentFileIndex == 0) ? userData.songdataA : userData.songdataB;
        if (!userData.currentSongData)
        {
                setEofHandled();
                return 0;
        }

        char *filePath = strdup(userData.currentSongData->filePath);
        if (!validFilePath(filePath))
        {
                free(filePath);
                setEofReached();
                return -1;
        }

        const CodecOps *ops = findCodecOps(filePath);
        if (!ops)
        {
                free(filePath);
                return -1;
        }

        if (ops->implType == BUILTIN)
                handleBuiltinAvgBitRate(audioData, userData.currentSongData, filePath);

        int result = handleCodec(filePath, *ops, audioData, userData, state, &context);
        free(filePath);

        if (result < 0)
        {
                setCurrentImplementationType(NONE);
                setImplSwitchNotReached();
                setEofReached();
                return -1;
        }

        setEofHandled();
        return 0;
}


bool isContextInitialized(void) { return contextInitialized; }

void cleanupAudioContext(void)
{
        ma_context_uninit(&context);
        contextInitialized = false;
}

int createAudioDevice(void)
{
        PlaybackState *ps = getPlaybackState();

        if (contextInitialized)
        {
                ma_context_uninit(&context);
                contextInitialized = false;
        }
        ma_context_init(NULL, 0, NULL, &context);
        contextInitialized = true;

        if (switchAudioImplementation() >= 0)
        {
                ps->notifySwitch = true;
        }
        else
        {
                return -1;
        }

        return 0;
}
