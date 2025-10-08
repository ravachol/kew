#define MA_EXPERIMENTAL__DATA_LOOPING_AND_CHAINING
#define MA_NO_ENGINE
#define MINIAUDIO_IMPLEMENTATION

#include "sound.h"
#include "file.h"  
#include <miniaudio.h>

/*

sound.c

 Functions related to miniaudio implementation

*/

ma_context context;

bool isContextInitialized = false;

bool tryAgain = false;

UserData userData;

ma_result initFirstDatasource(AudioData *pAudioData, UserData *pUserData)
{
        char *filePath = NULL;

        SongData *songData = (pAudioData->currentFileIndex == 0)
                                 ? pUserData->songdataA
                                 : pUserData->songdataB;

        if (songData == NULL)
        {
                return MA_ERROR;
        }

        filePath = songData->filePath;

        if (filePath == NULL)
        {
                return MA_ERROR;
        }

        pAudioData->pUserData = pUserData;
        pAudioData->currentPCMFrame = 0;
        pAudioData->restart = false;

        if (hasBuiltinDecoder(filePath))
        {
                int result = prepareNextDecoder(filePath);
                if (result < 0)
                        return -1;
                ma_decoder *first = getFirstDecoder();
                pAudioData->format = first->outputFormat;
                pAudioData->channels = first->outputChannels;
                pAudioData->sampleRate = first->outputSampleRate;
                ma_data_source_get_length_in_pcm_frames(
                    first, &(pAudioData->totalFrames));
        }
        else if (pathEndsWith(filePath, "opus"))
        {
                int result = prepareNextOpusDecoder(filePath);
                if (result < 0)
                        return -1;
                ma_libopus *first = getFirstOpusDecoder();
                ma_channel channelMap[MA_MAX_CHANNELS];
                ma_libopus_ds_get_data_format(
                    first, &(pAudioData->format), &(pAudioData->channels),
                    &(pAudioData->sampleRate), channelMap, MA_MAX_CHANNELS);
                ma_data_source_get_length_in_pcm_frames(
                    first, &(pAudioData->totalFrames));
                ma_data_source_base *base = (ma_data_source_base *)first;
                base->pCurrent = first;
                first->pReadSeekTellUserData = pAudioData;
        }
        else if (pathEndsWith(filePath, "ogg"))
        {
                int result = prepareNextVorbisDecoder(filePath);
                if (result < 0)
                        return -1;
                ma_libvorbis *first = getFirstVorbisDecoder();
                ma_channel channelMap[MA_MAX_CHANNELS];
                ma_libvorbis_ds_get_data_format(
                    first, &(pAudioData->format), &(pAudioData->channels),
                    &(pAudioData->sampleRate), channelMap, MA_MAX_CHANNELS);
                ma_data_source_get_length_in_pcm_frames(
                    first, &(pAudioData->totalFrames));
                ma_data_source_base *base = (ma_data_source_base *)first;
                base->pCurrent = first;
                first->pReadSeekTellUserData = pAudioData;
        }
        else if (pathEndsWith(filePath, "webm"))
        {
                int result = prepareNextWebmDecoder(songData);
                if (result < 0)
                        return -1;
                ma_webm *first = getFirstWebmDecoder();
                ma_channel channelMap[MA_MAX_CHANNELS];
                ma_webm_ds_get_data_format(
                    first, &(pAudioData->format), &(pAudioData->channels),
                    &(pAudioData->sampleRate), channelMap, MA_MAX_CHANNELS);
                ma_data_source_get_length_in_pcm_frames(
                    first, &(pAudioData->totalFrames));
                ma_data_source_base *base = (ma_data_source_base *)first;
                base->pCurrent = first;
                first->pReadSeekTellUserData = pAudioData;
        }
        else if (pathEndsWith(filePath, "m4a") || pathEndsWith(filePath, "aac"))
        {
#ifdef USE_FAAD

                int result = prepareNextM4aDecoder(songData);
                if (result < 0)
                        return -1;
                m4a_decoder *first = getFirstM4aDecoder();
                ma_channel channelMap[MA_MAX_CHANNELS];
                m4a_decoder_ds_get_data_format(
                    first, &(pAudioData->format), &(pAudioData->channels),
                    &(pAudioData->sampleRate), channelMap, MA_MAX_CHANNELS);
                ma_data_source_get_length_in_pcm_frames(
                    first, &(pAudioData->totalFrames));
                ma_data_source_base *base = (ma_data_source_base *)first;
                base->pCurrent = first;
                first->pReadSeekTellUserData = pAudioData;
#else
                return MA_ERROR;
#endif
        }
        else
        {
                return MA_ERROR;
        }

        return MA_SUCCESS;
}

int createDevice(UserData *userData, ma_device *device, ma_context *context,
                 ma_data_source_vtable *vtable, ma_device_data_proc callback)
{
        ma_result result;

        result = initFirstDatasource(&audioData, userData);
        if (result != MA_SUCCESS)
                return -1;

        audioData.base.vtable = vtable;

        ma_device_config deviceConfig =
            ma_device_config_init(ma_device_type_playback);
        deviceConfig.playback.format = audioData.format;
        deviceConfig.playback.channels = audioData.channels;
        deviceConfig.sampleRate = audioData.sampleRate;
        deviceConfig.dataCallback = callback;
        deviceConfig.pUserData = &audioData;

        result = ma_device_init(context, &deviceConfig, device);
        if (result != MA_SUCCESS)
        {
                return -1;
        }

        setVolume(getCurrentVolume());

        result = ma_device_start(device);
        if (result != MA_SUCCESS)
        {
                ma_device_uninit(device);
                return -1;
        }

        appState.uiState.doNotifyMPRISPlaying = true;

        return 0;
}

int builtin_createAudioDevice(UserData *userData, ma_device *device,
                              ma_context *context,
                              ma_data_source_vtable *vtable)
{
        return createDevice(userData, device, context, vtable,
                            builtin_on_audio_frames);
}

int vorbis_createAudioDevice(UserData *userData, ma_device *device,
                             ma_context *context)
{
        ma_result result;

        result = initFirstDatasource(&audioData, userData);
        if (result != MA_SUCCESS)
        {
                printf("\n\nFailed to initialize ogg vorbis file.\n");
                return -1;
        }
        ma_libvorbis *vorbis = getFirstVorbisDecoder();
        ma_device_config deviceConfig =
            ma_device_config_init(ma_device_type_playback);

        deviceConfig.playback.format = vorbis->format;
        deviceConfig.playback.channels = audioData.channels;
        deviceConfig.sampleRate = audioData.sampleRate;
        deviceConfig.dataCallback = vorbis_on_audio_frames;
        deviceConfig.pUserData = vorbis;

        result = ma_device_init(context, &deviceConfig, device);
        if (result != MA_SUCCESS)
        {
                setErrorMessage("Failed to initialize miniaudio device.");
                return -1;
        }

        setVolume(getCurrentVolume());

        result = ma_device_start(device);
        if (result != MA_SUCCESS)
        {
                setErrorMessage("Failed to start miniaudio device.");
                return -1;
        }

        appState.uiState.doNotifyMPRISPlaying = true;

        return 0;
}

#ifdef USE_FAAD
int m4a_createAudioDevice(UserData *userData, ma_device *device,
                          ma_context *context)
{
        ma_result result;

        result = initFirstDatasource(&audioData, userData);
        if (result != MA_SUCCESS)
        {
                if (!hasErrorMessage())
                        setErrorMessage("M4a type not supported.");
                return -1;
        }
        m4a_decoder *decoder = getFirstM4aDecoder();
        ma_device_config deviceConfig =
            ma_device_config_init(ma_device_type_playback);

        deviceConfig.playback.format = decoder->format;
        deviceConfig.playback.channels = audioData.channels;
        deviceConfig.sampleRate = audioData.sampleRate;
        deviceConfig.dataCallback = m4a_on_audio_frames;
        deviceConfig.pUserData = decoder;

        result = ma_device_init(context, &deviceConfig, device);
        if (result != MA_SUCCESS)
        {
                setErrorMessage("Failed to initialize miniaudio device.");
                return -1;
        }

        setVolume(getCurrentVolume());

        result = ma_device_start(device);

        if (result != MA_SUCCESS)
        {
                setErrorMessage("Failed to start miniaudio device.");
                return -1;
        }

        appState.uiState.doNotifyMPRISPlaying = true;

        return 0;
}
#endif

int opus_createAudioDevice(UserData *userData, ma_device *device,
                           ma_context *context)
{
        ma_result result;

        result = initFirstDatasource(&audioData, userData);
        if (result != MA_SUCCESS)
        {
                printf("\n\nFailed to initialize opus file.\n");
                return -1;
        }
        ma_libopus *opus = getFirstOpusDecoder();

        ma_device_config deviceConfig =
            ma_device_config_init(ma_device_type_playback);

        deviceConfig.playback.format = opus->format;
        deviceConfig.playback.channels = audioData.channels;
        deviceConfig.sampleRate = audioData.sampleRate;
        deviceConfig.dataCallback = opus_on_audio_frames;
        deviceConfig.pUserData = opus;

        result = ma_device_init(context, &deviceConfig, device);
        if (result != MA_SUCCESS)
        {
                setErrorMessage("Failed to initialize miniaudio device.");
                return -1;
        }

        setVolume(getCurrentVolume());

        result = ma_device_start(device);
        if (result != MA_SUCCESS)
        {
                setErrorMessage("Failed to start miniaudio device.");
                return -1;
        }

        appState.uiState.doNotifyMPRISPlaying = true;

        return 0;
}

int webm_createAudioDevice(UserData *userData, ma_device *device,
                           ma_context *context)
{
        ma_result result;

        result = initFirstDatasource(&audioData, userData);
        if (result != MA_SUCCESS)
        {
                printf("\n\nFailed to initialize webm file.\n");
                return -1;
        }
        ma_webm *webm = getFirstWebmDecoder();
        ma_device_config deviceConfig =
            ma_device_config_init(ma_device_type_playback);

        deviceConfig.playback.format = audioData.format;
        deviceConfig.playback.channels = audioData.channels;
        deviceConfig.sampleRate = audioData.sampleRate;
        deviceConfig.dataCallback = webm_on_audio_frames;
        deviceConfig.pUserData = webm;

        result = ma_device_init(context, &deviceConfig, device);
        if (result != MA_SUCCESS)
        {
                setErrorMessage("Failed to initialize miniaudio device.");
                return -1;
        }

        setVolume(getCurrentVolume());

        result = ma_device_start(device);
        if (result != MA_SUCCESS)
        {
                setErrorMessage("Failed to start miniaudio device.");
                return -1;
        }

        appState.uiState.doNotifyMPRISPlaying = true;

        return 0;
}

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

int calcAvgBitRate(double duration, const char *filePath)
{
        long long fileSize = getFileSize(filePath); // in bytes
        int avgBitRate = 0;

        if (duration > 0.0)
                avgBitRate = (int)((fileSize * 8.0) / duration /
                                   1000.0); // use 1000 for kbps

        return avgBitRate;
}

int switchAudioImplementation(void)
{
        if (audioData.endOfListReached)
        {
                setEOFNotReached();
                setCurrentImplementationType(NONE);
                return 0;
        }

        enum AudioImplementation currentImplementation =
            getCurrentImplementationType();

        userData.currentSongData = (audioData.currentFileIndex == 0)
                                       ? userData.songdataA
                                       : userData.songdataB;

        char *filePath = NULL;

        if (appState.current_lyrics) {
                free_lyrics(appState.current_lyrics);
                appState.current_lyrics = NULL;
        }

        if (userData.currentSongData && userData.currentSongData->filePath[0] != '\0') {
                appState.current_lyrics = load_lyrics(userData.currentSongData->filePath);
                if (appState.current_lyrics) {
                    printf("✅ Lirik dimuat: %d baris\n", appState.current_lyrics->count);
                } else {
                    printf("❌ Lirik gagal dimuat\n");
                }
        }

        if (userData.currentSongData == NULL)
        {
                setEOFNotReached();
                return 0;
        }
        else
        {

                if (!validFilePath(userData.currentSongData->filePath))
                {
                        if (!tryAgain)
                        {
                                setCurrentFileIndex(
                                    &audioData, 1 - audioData.currentFileIndex);
                                tryAgain = true;
                                switchAudioImplementation();
                                return 0;
                        }
                        else
                        {
                                setEOFReached();
                                return -1;
                        }
                }

                filePath = strdup(userData.currentSongData->filePath);
        }

        tryAgain = false;

        if (hasBuiltinDecoder(filePath))
        {
                ma_uint32 sampleRate = 0;
                ma_uint32 channels = 0;
                ma_format format = ma_format_unknown;
                ma_decoder *decoder = getCurrentBuiltinDecoder();

                getFileInfo(filePath, &sampleRate, &channels, &format);

                bool sameFormat = (decoder != NULL &&
                                   (sampleRate == decoder->outputSampleRate &&
                                    channels == decoder->outputChannels &&
                                    format == decoder->outputFormat));

                if (pathEndsWith(filePath, ".mp3") && userData.currentSongData)
                {
                        int avgBitRate = calcAvgBitRate(
                            userData.currentSongData->duration, filePath);

                        if (avgBitRate > 320)
                                avgBitRate = 320;

                        userData.currentSongData->avgBitRate =
                            audioData.avgBitRate = avgBitRate;
                }
                else
                        audioData.avgBitRate = 0;

                if (isRepeatEnabled() ||
                    !(sameFormat && currentImplementation == BUILTIN))
                {
                        setImplSwitchReached();

                        pthread_mutex_lock(&dataSourceMutex);

                        setCurrentImplementationType(BUILTIN);

                        cleanupPlaybackDevice();

                        resetAllDecoders();
                        resetAudioBuffer();

                        audioData.sampleRate = sampleRate;

                        int result = builtin_createAudioDevice(
                            &userData, getDevice(), &context,
                            &builtin_file_data_source_vtable);

                        if (result < 0)
                        {
                                setCurrentImplementationType(NONE);
                                setImplSwitchNotReached();
                                setEOFReached();
                                free(filePath);
                                pthread_mutex_unlock(&dataSourceMutex);
                                return -1;
                        }

                        pthread_mutex_unlock(&dataSourceMutex);

                        setImplSwitchNotReached();
                }
        }
        else if (pathEndsWith(filePath, "opus"))
        {
                ma_uint32 sampleRate;
                ma_uint32 channels;
                ma_format format;
                ma_channel channelMap[MA_MAX_CHANNELS];

                ma_uint32 nSampleRate;
                ma_uint32 nChannels;
                ma_format nFormat;
                ma_channel nChannelMap[MA_MAX_CHANNELS];
                ma_libopus *decoder = getCurrentOpusDecoder();

                getOpusFileInfo(filePath, &format, &channels, &sampleRate,
                                channelMap);

                if (decoder != NULL)
                        ma_libopus_ds_get_data_format(
                            decoder, &nFormat, &nChannels, &nSampleRate,
                            nChannelMap, MA_MAX_CHANNELS);

                bool sameFormat =
                    (decoder != NULL &&
                     (format == decoder->format && channels == nChannels &&
                      sampleRate == nSampleRate));

                if (isRepeatEnabled() ||
                    !(sameFormat && currentImplementation == OPUS))
                {
                        setImplSwitchReached();

                        pthread_mutex_lock(&dataSourceMutex);

                        setCurrentImplementationType(OPUS);

                        cleanupPlaybackDevice();

                        resetAllDecoders();
                        resetAudioBuffer();

                        audioData.sampleRate = sampleRate;
                        audioData.avgBitRate = 0;

                        int result = opus_createAudioDevice(
                            &userData, getDevice(), &context);

                        if (result < 0)
                        {
                                setCurrentImplementationType(NONE);
                                setImplSwitchNotReached();
                                setEOFReached();
                                free(filePath);
                                pthread_mutex_unlock(&dataSourceMutex);
                                return -1;
                        }

                        pthread_mutex_unlock(&dataSourceMutex);

                        setImplSwitchNotReached();
                }
        }
        else if (pathEndsWith(filePath, "ogg"))
        {
                ma_uint32 sampleRate;
                ma_uint32 channels;
                ma_format format;
                ma_channel channelMap[MA_MAX_CHANNELS];

                ma_uint32 nSampleRate;
                ma_uint32 nChannels;
                ma_format nFormat;
                ma_channel nChannelMap[MA_MAX_CHANNELS];
                ma_libvorbis *decoder = getCurrentVorbisDecoder();

                getVorbisFileInfo(filePath, &format, &channels, &sampleRate,
                                  channelMap);

                if (decoder != NULL)
                        ma_libvorbis_ds_get_data_format(
                            decoder, &nFormat, &nChannels, &nSampleRate,
                            nChannelMap, MA_MAX_CHANNELS);

                bool sameFormat =
                    (decoder != NULL &&
                     (format == decoder->format && channels == nChannels &&
                      sampleRate == nSampleRate));

                if (userData.currentSongData)
                        userData.currentSongData->avgBitRate =
                            audioData.avgBitRate = calcAvgBitRate(
                                userData.currentSongData->duration, filePath);
                else
                        audioData.avgBitRate = 0;

                if (isRepeatEnabled() ||
                    !(sameFormat && currentImplementation == VORBIS))
                {
                        setImplSwitchReached();

                        pthread_mutex_lock(&dataSourceMutex);

                        setCurrentImplementationType(VORBIS);

                        cleanupPlaybackDevice();

                        resetAllDecoders();
                        resetAudioBuffer();

                        audioData.sampleRate = sampleRate;

                        int result = vorbis_createAudioDevice(
                            &userData, getDevice(), &context);

                        if (result < 0)
                        {
                                setCurrentImplementationType(NONE);
                                setImplSwitchNotReached();
                                setEOFReached();
                                free(filePath);
                                pthread_mutex_unlock(&dataSourceMutex);
                                return -1;
                        }

                        pthread_mutex_unlock(&dataSourceMutex);

                        setImplSwitchNotReached();
                }
        }
        else if (pathEndsWith(filePath, "webm"))
        {
                ma_uint32 sampleRate;
                ma_uint32 channels;
                ma_format format;
                ma_channel channelMap[MA_MAX_CHANNELS];

                ma_uint32 nSampleRate;
                ma_uint32 nChannels;
                ma_format nFormat;
                ma_channel nChannelMap[MA_MAX_CHANNELS];
                ma_webm *decoder = getCurrentWebmDecoder();

                getWebmFileInfo(filePath, &format, &channels, &sampleRate,
                                channelMap);

                if (decoder != NULL)
                        ma_webm_ds_get_data_format(
                            decoder, &nFormat, &nChannels, &nSampleRate,
                            nChannelMap, MA_MAX_CHANNELS);

                bool sameFormat = false;

                // FIXME: Gapless/chaining of decoders disabled for now
                // bool sameFormat = (decoder != NULL && (format ==
                // decoder->format &&
                //                                       channels == nChannels
                //                                       && sampleRate ==
                //                                       nSampleRate));

                audioData.avgBitRate = 0;

                if (isRepeatEnabled() ||
                    !(sameFormat && currentImplementation == WEBM))
                {
                        setImplSwitchReached();

                        pthread_mutex_lock(&dataSourceMutex);

                        setCurrentImplementationType(WEBM);

                        cleanupPlaybackDevice();

                        resetAllDecoders();
                        resetAudioBuffer();

                        audioData.sampleRate = sampleRate;

                        int result = webm_createAudioDevice(
                            &userData, getDevice(), &context);

                        if (result < 0)
                        {
                                setCurrentImplementationType(NONE);
                                setImplSwitchNotReached();
                                setEOFReached();
                                free(filePath);
                                pthread_mutex_unlock(&dataSourceMutex);
                                return -1;
                        }

                        pthread_mutex_unlock(&dataSourceMutex);

                        setImplSwitchNotReached();
                }
        }
        else if (pathEndsWith(filePath, "m4a") || pathEndsWith(filePath, "aac"))
        {
#ifdef USE_FAAD
                ma_uint32 sampleRate;
                ma_uint32 channels;
                ma_format format;
                ma_channel channelMap[MA_MAX_CHANNELS];

                ma_uint32 nSampleRate;
                ma_uint32 nChannels;
                ma_format nFormat;
                int avgBitRate;
                ma_channel nChannelMap[MA_MAX_CHANNELS];
                m4a_decoder *decoder = getCurrentM4aDecoder();
                k_m4adec_filetype fileType = k_unknown;

                getM4aFileInfo(filePath, &format, &channels, &sampleRate,
                               channelMap, &avgBitRate, &fileType);

                if (decoder != NULL)
                        m4a_decoder_ds_get_data_format(
                            decoder, &nFormat, &nChannels, &nSampleRate,
                            nChannelMap, MA_MAX_CHANNELS);

                bool sameFormat =
                    (decoder != NULL &&
                     (format == decoder->format && channels == nChannels &&
                      sampleRate == nSampleRate &&
                      decoder->fileType == fileType &&
                      decoder->fileType != k_rawAAC));

                if (userData.currentSongData)
                        userData.currentSongData->avgBitRate =
                            audioData.avgBitRate = avgBitRate;

                if (isRepeatEnabled() ||
                    !(sameFormat && currentImplementation == M4A))
                {
                        setImplSwitchReached();

                        pthread_mutex_lock(&dataSourceMutex);

                        setCurrentImplementationType(M4A);

                        cleanupPlaybackDevice();

                        resetAllDecoders();
                        resetAudioBuffer();

                        audioData.sampleRate = sampleRate;

                        int result = m4a_createAudioDevice(
                            &userData, getDevice(), &context);

                        if (result < 0)
                        {
                                setCurrentImplementationType(NONE);
                                setImplSwitchNotReached();
                                setEOFReached();
                                free(filePath);
                                pthread_mutex_unlock(&dataSourceMutex);
                                return -1;
                        }

                        pthread_mutex_unlock(&dataSourceMutex);

                        setImplSwitchNotReached();
                }
#else
                setCurrentImplementationType(NONE);
                setImplSwitchNotReached();
                setEOFReached();
                free(filePath);
                pthread_mutex_unlock(&dataSourceMutex);
                return -1;
#endif
        }
        else
        {
                free(filePath);
                return -1;
        }

        free(filePath);
        setEOFNotReached();

        return 0;
}

void cleanupAudioContext(void)
{
        ma_context_uninit(&context);
        isContextInitialized = false;
}

int createAudioDevice()
{
        if (isContextInitialized)
        {
                ma_context_uninit(&context);
                isContextInitialized = false;
        }
        ma_context_init(NULL, 0, NULL, &context);
        isContextInitialized = true;

        if (switchAudioImplementation() >= 0)
        {
                appState.uiState.doNotifyMPRISSwitched = true;
        }
        else
        {
                return -1;
        }

        return 0;
}

void resumePlayback(void)
{
        // If this was unpaused with no song loaded
        if (audioData.restart)
        {
                audioData.endOfListReached = false;
        }

        if (!ma_device_is_started(&device))
        {
                if (ma_device_start(&device) != MA_SUCCESS)
                {
                        createAudioDevice();
                        ma_device_start(&device);
                }
        }

        paused = false;

        stopped = false;

        if (appState.currentView != TRACK_VIEW)
        {
                refresh = true;
        }
}

float get_current_playback_time(void) {
    ma_uint64 cursor = 0;
    ma_uint32 sampleRate = audioData.sampleRate;

    if (sampleRate == 0) {
        return 0.0f;
    }

    enum AudioImplementation currentImplementation = getCurrentImplementationType();

    switch (currentImplementation) {
        case BUILTIN: {
            ma_decoder* decoder = getCurrentBuiltinDecoder();
            if (decoder) {
                ma_decoder_get_cursor_in_pcm_frames(decoder, &cursor);
            }
            break;
        }
        case OPUS: {
            ma_libopus* decoder = getCurrentOpusDecoder();
            if (decoder) {
                ma_data_source_get_cursor_in_pcm_frames(decoder, &cursor);
            }
            break;
        }
        case VORBIS: {
            ma_libvorbis* decoder = getCurrentVorbisDecoder();
            if (decoder) {
                ma_data_source_get_cursor_in_pcm_frames(decoder, &cursor);
            }
            break;
        }
        case WEBM: {
            ma_webm* decoder = getCurrentWebmDecoder();
            if (decoder) {
                ma_data_source_get_cursor_in_pcm_frames(decoder, &cursor);
            }
            break;
        }
#ifdef USE_FAAD
        case M4A: {
            m4a_decoder* decoder = getCurrentM4aDecoder();
            if (decoder) {
                ma_data_source_get_cursor_in_pcm_frames(decoder, &cursor);
            }
            break;
        }
#endif
        default:
            return 0.0f;
    }

    float time = (float)cursor / (float)sampleRate;
    return time;
}
