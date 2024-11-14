#define MA_EXPERIMENTAL__DATA_LOOPING_AND_CHAINING
#define MA_NO_ENGINE
#define MINIAUDIO_IMPLEMENTATION

#include <miniaudio.h>
#include "sound.h"

/*

sound.c

 Functions related to miniaudio implementation

*/

ma_context context;

bool isContextInitialized = false;

UserData userData;

ma_result initFirstDatasource(AudioData *pAudioData, UserData *pUserData)
{
        char *filePath = NULL;

        filePath = (pAudioData->currentFileIndex == 0) ? pUserData->songdataA->filePath : pUserData->songdataB->filePath;

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
                ma_data_source_get_length_in_pcm_frames(first, &pAudioData->totalFrames);
        }
        else if (pathEndsWith(filePath, "opus"))
        {
                int result = prepareNextOpusDecoder(filePath);
                if (result < 0)
                        return -1;
                ma_libopus *first = getFirstOpusDecoder();
                ma_channel channelMap[MA_MAX_CHANNELS];
                ma_libopus_ds_get_data_format(first, &pAudioData->format, &pAudioData->channels, &pAudioData->sampleRate, channelMap, MA_MAX_CHANNELS);
                ma_data_source_get_length_in_pcm_frames(first, &pAudioData->totalFrames);
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
                ma_libvorbis_ds_get_data_format(first, &pAudioData->format, &pAudioData->channels, &pAudioData->sampleRate, channelMap, MA_MAX_CHANNELS);
                ma_data_source_get_length_in_pcm_frames(first, &pAudioData->totalFrames);
                ma_data_source_base *base = (ma_data_source_base *)first;
                base->pCurrent = first;
                first->pReadSeekTellUserData = pAudioData;
        }
        else if (pathEndsWith(filePath, "m4a") || pathEndsWith(filePath, "aac"))
        {
#ifdef USE_FAAD

                int result = prepareNextM4aDecoder(filePath);
                if (result < 0)
                        return -1;
                m4a_decoder *first = getFirstM4aDecoder();
                ma_channel channelMap[MA_MAX_CHANNELS];
                m4a_decoder_ds_get_data_format(first, &pAudioData->format, &pAudioData->channels, &pAudioData->sampleRate, channelMap, MA_MAX_CHANNELS);
                ma_data_source_get_length_in_pcm_frames(first, &pAudioData->totalFrames);
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

int createDevice(UserData *userData, ma_device *device, ma_context *context, ma_data_source_vtable *vtable, ma_device_data_proc callback)
{
        ma_result result;

        ma_data_source_uninit(&audioData);
        result = initFirstDatasource(&audioData, userData);
        if (result != MA_SUCCESS)
                return -1;

        audioData.base.vtable = vtable;

        ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
        deviceConfig.playback.format = audioData.format;
        deviceConfig.playback.channels = audioData.channels;
        deviceConfig.sampleRate = audioData.sampleRate;
        deviceConfig.dataCallback = callback;
        deviceConfig.pUserData = &audioData;

        result = ma_device_init(context, &deviceConfig, device);
        if (result != MA_SUCCESS)
                return -1;

        setVolume(getCurrentVolume());

        result = ma_device_start(device);
        if (result != MA_SUCCESS)
                return -1;        

        appState.uiState.doNotifyMPRISPlaying = true;

        return 0;
}

int builtin_createAudioDevice(UserData *userData, ma_device *device, ma_context *context, ma_data_source_vtable *vtable)
{
        return createDevice(userData, device, context, vtable, builtin_on_audio_frames);
}

int vorbis_createAudioDevice(UserData *userData, ma_device *device, ma_context *context)
{
        ma_result result;

        result = initFirstDatasource(&audioData, userData);
        if (result != MA_SUCCESS)
        {
                printf("\n\nFailed to initialize ogg vorbis file.\n");
                return -1;
        }
        ma_libvorbis *vorbis = getFirstVorbisDecoder();
        ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);

        deviceConfig.playback.format = vorbis->format;
        deviceConfig.playback.channels = audioData.channels;
        deviceConfig.sampleRate = audioData.sampleRate;
        deviceConfig.dataCallback = vorbis_on_audio_frames;
        deviceConfig.pUserData = vorbis;

        result = ma_device_init(context, &deviceConfig, device);
        if (result != MA_SUCCESS)
        {
                printf("\n\nFailed to initialize miniaudio device.\n");
                return -1;
        }

        setVolume(getCurrentVolume());

        result = ma_device_start(device);
        if (result != MA_SUCCESS)
        {
                printf("\n\nFailed to start miniaudio device.\n");
                return -1;
        }
        
        appState.uiState.doNotifyMPRISPlaying = true;

        return 0;
}

#ifdef USE_FAAD
int m4a_createAudioDevice(UserData *userData, ma_device *device, ma_context *context)
{
        ma_result result;

        result = initFirstDatasource(&audioData, userData);
        if (result != MA_SUCCESS)
        {
                printf("\n\nFailed to initialize m4a file.\n");
                return -1;
        }
        m4a_decoder *decoder = getFirstM4aDecoder();
        ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);

        deviceConfig.playback.format = decoder->format;
        deviceConfig.playback.channels = audioData.channels;
        deviceConfig.sampleRate = audioData.sampleRate;
        deviceConfig.dataCallback = m4a_on_audio_frames;
        deviceConfig.pUserData = decoder;

        result = ma_device_init(context, &deviceConfig, device);
        if (result != MA_SUCCESS)
        {
                printf("\n\nFailed to initialize miniaudio device.\n");
                return -1;
        }

        setVolume(getCurrentVolume());

        result = ma_device_start(device);

        if (result != MA_SUCCESS)
        {
                printf("\n\nFailed to start miniaudio device.\n");
                return -1;
        }
       
        appState.uiState.doNotifyMPRISPlaying = true;

        return 0;
}
#endif

int opus_createAudioDevice(UserData *userData, ma_device *device, ma_context *context)
{
        ma_result result;

        result = initFirstDatasource(&audioData, userData);
        if (result != MA_SUCCESS)
        {
                printf("\n\nFailed to initialize opus file.\n");
                return -1;
        }
        ma_libopus *opus = getFirstOpusDecoder();

        ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);

        deviceConfig.playback.format = opus->format;
        deviceConfig.playback.channels = audioData.channels;
        deviceConfig.sampleRate = audioData.sampleRate;
        deviceConfig.dataCallback = opus_on_audio_frames;
        deviceConfig.pUserData = opus;

        result = ma_device_init(context, &deviceConfig, device);
        if (result != MA_SUCCESS)
        {
                printf("\n\nFailed to initialize miniaudio device.\n");
                return -1;
        }

        setVolume(getCurrentVolume());

        result = ma_device_start(device);
        if (result != MA_SUCCESS)
        {
                printf("\n\nFailed to start miniaudio device.\n");
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

bool tryAgain = false;

int switchAudioImplementation(void)
{
        if (audioData.endOfListReached)
        {
                setEOFNotReached();
                setCurrentImplementationType(NONE);
                return 0;
        }

        enum AudioImplementation currentImplementation = getCurrentImplementationType();

        userData.currentSongData = (audioData.currentFileIndex == 0) ? userData.songdataA : userData.songdataB;

        char *filePath = NULL;

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
                                setCurrentFileIndex(&audioData, 1 - audioData.currentFileIndex);
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

                bool sameFormat = (decoder != NULL && (sampleRate == decoder->outputSampleRate &&
                                                       channels == decoder->outputChannels &&
                                                       format == decoder->outputFormat));

                if (isRepeatEnabled() || !(sameFormat && currentImplementation == BUILTIN))
                {
                        setImplSwitchReached();

                        pthread_mutex_lock(&dataSourceMutex);

                        setCurrentImplementationType(BUILTIN);

                        cleanupPlaybackDevice();

                        resetDecoders();
                        resetVorbisDecoders();
#ifdef USE_FAAD                        
                        resetM4aDecoders();
#endif                        
                        resetOpusDecoders();
                        resetAudioBuffer();

                        int result = builtin_createAudioDevice(&userData, getDevice(), &context, &builtin_file_data_source_vtable);

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

                getOpusFileInfo(filePath, &format, &channels, &sampleRate, channelMap);

                if (decoder != NULL)
                        ma_libopus_ds_get_data_format(decoder, &nFormat, &nChannels, &nSampleRate, nChannelMap, MA_MAX_CHANNELS);

                bool sameFormat = (decoder != NULL && (format == decoder->format &&
                                                       channels == nChannels &&
                                                       sampleRate == nSampleRate));

                if (isRepeatEnabled() || !(sameFormat && currentImplementation == OPUS))
                {
                        setImplSwitchReached();

                        pthread_mutex_lock(&dataSourceMutex);

                        setCurrentImplementationType(OPUS);

                        cleanupPlaybackDevice();

                        resetDecoders();
                        resetVorbisDecoders();
#ifdef USE_FAAD                        
                        resetM4aDecoders();
#endif                        
                        resetOpusDecoders();
                        resetAudioBuffer();

                        int result = opus_createAudioDevice(&userData, getDevice(), &context);

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

                getVorbisFileInfo(filePath, &format, &channels, &sampleRate, channelMap);

                if (decoder != NULL)
                        ma_libvorbis_ds_get_data_format(decoder, &nFormat, &nChannels, &nSampleRate, nChannelMap, MA_MAX_CHANNELS);

                bool sameFormat = (decoder != NULL && (format == decoder->format &&
                                                       channels == nChannels &&
                                                       sampleRate == nSampleRate));

                if (isRepeatEnabled() || !(sameFormat && currentImplementation == VORBIS))
                {
                        setImplSwitchReached();

                        pthread_mutex_lock(&dataSourceMutex);

                        setCurrentImplementationType(VORBIS);

                        cleanupPlaybackDevice();

                        resetDecoders();
                        resetVorbisDecoders();
#ifdef USE_FAD                        
                        resetM4aDecoders();
#endif                        
                        resetOpusDecoders();
                        resetAudioBuffer();

                        int result = vorbis_createAudioDevice(&userData, getDevice(), &context);

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
                ma_channel nChannelMap[MA_MAX_CHANNELS];
                m4a_decoder *decoder = getCurrentM4aDecoder();

                getM4aFileInfo(filePath, &format, &channels, &sampleRate, channelMap);

                if (decoder != NULL)
                        m4a_decoder_ds_get_data_format(decoder, &nFormat, &nChannels, &nSampleRate, nChannelMap, MA_MAX_CHANNELS);

                bool sameFormat = (decoder != NULL && (format == decoder->format &&
                                                       channels == nChannels &&
                                                       sampleRate == nSampleRate));

                if (isRepeatEnabled() || !(sameFormat && currentImplementation == M4A))
                {
                        setImplSwitchReached();

                        pthread_mutex_lock(&dataSourceMutex);

                        setCurrentImplementationType(M4A);

                        cleanupPlaybackDevice();

                        resetDecoders();
                        resetVorbisDecoders();
                        resetM4aDecoders();
                        resetOpusDecoders();
                        resetAudioBuffer();

                        int result = m4a_createAudioDevice(&userData, getDevice(), &context);

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
