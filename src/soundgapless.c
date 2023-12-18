#define MA_EXPERIMENTAL__DATA_LOOPING_AND_CHAINING
#define MA_NO_ENGINE
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#include "soundgapless.h"

/*

soundgapless.c

 Functions related to miniaudio implementation

*/

ma_context context;

UserData userData;

AudioData audioData;

ma_result initFirstDatasource(AudioData *pAudioData, UserData *pUserData)
{
        char *filePath = NULL;

        filePath = (pAudioData->currentFileIndex == 0) ? pUserData->songdataA->filePath : pUserData->songdataB->filePath;

        pAudioData->pUserData = pUserData;
        pAudioData->currentPCMFrame = 0;

        if (hasBuiltinDecoder(filePath))
        {
                prepareNextDecoder(filePath);
                ma_decoder *first = getFirstDecoder();
                pAudioData->format = first->outputFormat;
                pAudioData->channels = first->outputChannels;
                pAudioData->sampleRate = first->outputSampleRate;
                ma_data_source_get_length_in_pcm_frames(first, &pAudioData->totalFrames);
        }
        else if (endsWith(filePath, "opus"))
        {
                prepareNextOpusDecoder(filePath);
                ma_libopus *first = getFirstOpusDecoder();
                ma_channel channelMap[MA_MAX_CHANNELS];
                ma_libopus_ds_get_data_format(first, &pAudioData->format, &pAudioData->channels, &pAudioData->sampleRate, channelMap, MA_MAX_CHANNELS);
                ma_data_source_get_length_in_pcm_frames(first, &pAudioData->totalFrames);
                ma_data_source_base *base = (ma_data_source_base *)first;
                base->pCurrent = first;
                first->pReadSeekTellUserData = pAudioData;
        }
        else if (endsWith(filePath, "ogg"))
        {
                prepareNextVorbisDecoder(filePath);
                ma_libvorbis *first = getFirstVorbisDecoder();
                ma_channel channelMap[MA_MAX_CHANNELS];
                ma_libvorbis_ds_get_data_format(first, &pAudioData->format, &pAudioData->channels, &pAudioData->sampleRate, channelMap, MA_MAX_CHANNELS);
                ma_data_source_get_length_in_pcm_frames(first, &pAudioData->totalFrames);
                ma_data_source_base *base = (ma_data_source_base *)first;
                base->pCurrent = first;
                first->pReadSeekTellUserData = pAudioData;
        }
        else
        {
                if ((pAudioData->currentFileIndex == 0) && pAudioData->fileA == NULL)
                {
                        pAudioData->filenameA = pUserData->filenameA;
                        pAudioData->fileA = fopen(pUserData->filenameA, "rb");
                }
                else if ((pAudioData->currentFileIndex == 1) && pAudioData->fileB == NULL)
                {
                        pAudioData->filenameB = pUserData->filenameB;
                        pAudioData->fileB = fopen(pUserData->filenameB, "rb");
                }

                pAudioData->format = SAMPLE_FORMAT;
                pAudioData->channels = CHANNELS;
                pAudioData->sampleRate = SAMPLE_RATE;
        }

        return MA_SUCCESS;
}

void createDevice(UserData *userData, ma_device *device, ma_context *context, ma_data_source_vtable *vtable, ma_device_data_proc callback)
{
        ma_result result;

        ma_data_source_uninit(&audioData);
        initFirstDatasource(&audioData, userData);

        audioData.base.vtable = vtable;

        ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
        deviceConfig.playback.format = audioData.format;
        deviceConfig.playback.channels = audioData.channels;
        deviceConfig.sampleRate = audioData.sampleRate;
        deviceConfig.dataCallback = callback;
        deviceConfig.pUserData = &audioData;

        result = ma_device_init(context, &deviceConfig, device);
        if (result != MA_SUCCESS)
                return;

        setVolume(getCurrentVolume());

        result = ma_device_start(device);
        if (result != MA_SUCCESS)
                return;
}

void builtin_createAudioDevice(UserData *userData, ma_device *device, ma_context *context, ma_data_source_vtable *vtable)
{
        createDevice(userData, device, context, vtable, builtin_on_audio_frames);
}

void pcm_createAudioDevice(UserData *userData, ma_device *device, ma_context *context, ma_data_source_vtable *vtable)
{
        createDevice(userData, device, context, vtable, pcm_on_audio_frames);
}

void vorbis_createAudioDevice(UserData *userData, ma_device *device, ma_context *context, ma_data_source_vtable *vtable)
{
        ma_result result;

        initFirstDatasource(&audioData, userData);
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
                printf("Failed to initialize miniaudio device.\n");
                return;
        }

        setVolume(getCurrentVolume());

        result = ma_device_start(device);
        if (result != MA_SUCCESS)
        {
                printf("Failed to start miniaudio device.\n");
                return;
        }
}

void opus_createAudioDevice(UserData *userData, ma_device *device, ma_context *context, ma_data_source_vtable *vtable)
{
        ma_result result;

        initFirstDatasource(&audioData, userData);
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
                printf("Failed to initialize miniaudio device.\n");
                return;
        }

        setVolume(getCurrentVolume());

        result = ma_device_start(device);
        if (result != MA_SUCCESS)
        {
                printf("Failed to start miniaudio device.\n");
                return;
        }
}

void cleanupAudioData()
{
        if (audioData.fileA != NULL)
                fclose(audioData.fileA);
        audioData.fileA = NULL;

        if (audioData.fileB != NULL)
                fclose(audioData.fileB);
        audioData.fileB = NULL;
}

void switchAudioImplementation()
{
        if (audioData.endOfListReached)
        {
                setEOFNotReached();
                setCurrentImplementationType(NONE);
                return;
        }

        enum AudioImplementation currentImplementation = getCurrentImplementationType();

        if (audioData.currentFileIndex == 0)
        {
                userData.currentSongData = userData.songdataA;
        }
        else
        {
                userData.currentSongData = userData.songdataB;
        }

        if (userData.currentSongData == NULL)
        {
                setEOFNotReached();
                return;
        }

        char *filePath = strdup(userData.currentSongData->filePath);

        if (filePath == NULL || filePath[0] == '\0' || filePath[0] == '\r')
        {
                free(filePath);
                setEOFNotReached();
                return;
        }

        if (hasBuiltinDecoder(filePath))
        {
                ma_uint32 sampleRate;
                ma_uint32 channels;
                ma_format format;
                ma_decoder *decoder = getCurrentDecoder();

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
                        resetOpusDecoders();
                        resetAudioBuffer();

                        builtin_createAudioDevice(&userData, getDevice(), &context, &builtin_file_data_source_vtable);

                        pthread_mutex_unlock(&dataSourceMutex);

                        setImplSwitchNotReached();
                }
        }
        else if (endsWith(filePath, "opus"))
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
                        resetOpusDecoders();
                        resetAudioBuffer();

                        opus_createAudioDevice(&userData, getDevice(), &context, &pcm_file_data_source_vtable);

                        pthread_mutex_unlock(&dataSourceMutex);

                        setImplSwitchNotReached();
                }
        }
        else if (endsWith(filePath, "ogg"))
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
                        resetOpusDecoders();
                        resetAudioBuffer();

                        vorbis_createAudioDevice(&userData, getDevice(), &context, &pcm_file_data_source_vtable);

                        pthread_mutex_unlock(&dataSourceMutex);

                        setImplSwitchNotReached();
                }
        }
        else
        {
                if (isRepeatEnabled() || currentImplementation != PCM)
                {
                        setImplSwitchReached();

                        pthread_mutex_lock(&dataSourceMutex);

                        setCurrentImplementationType(PCM);

                        cleanupPlaybackDevice();

                        resetDecoders();
                        resetVorbisDecoders();
                        resetOpusDecoders();
                        resetAudioBuffer();

                        pcm_createAudioDevice(&userData, getDevice(), &context, &pcm_file_data_source_vtable);

                        pthread_mutex_unlock(&dataSourceMutex);

                        setImplSwitchNotReached();
                }
        }
        free(filePath);
        setEOFNotReached();
}

void cleanupAudioContext()
{
        ma_context_uninit(&context);
}

void createAudioDevice(UserData *userData)
{
        ma_context_init(NULL, 0, NULL, &context);
        switchAudioImplementation();
}
