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
UserData *g_userData;
PCMFileDataSource pcmDataSource;

ma_result initFirstDatasource(PCMFileDataSource *pPCMDataSource, UserData *pUserData)
{
        char *filePath = NULL;

        filePath = (pPCMDataSource->currentFileIndex == 0) ? pUserData->songdataA->filePath : pUserData->songdataB->filePath;

        pPCMDataSource->pUserData = pUserData;
        pPCMDataSource->currentPCMFrame = 0;

        if (hasBuiltinDecoder(filePath))
        {
                prepareNextDecoder(filePath);
                ma_decoder *first = getFirstDecoder();
                pPCMDataSource->format = first->outputFormat;
                pPCMDataSource->channels = first->outputChannels;
                pPCMDataSource->sampleRate = first->outputSampleRate;
                ma_data_source_get_length_in_pcm_frames(first, &pPCMDataSource->totalFrames);
        }
        else if (endsWith(filePath, "opus"))
        {
                prepareNextOpusDecoder(filePath);
                ma_libopus *first = getFirstOpusDecoder();
                ma_channel channelMap[MA_MAX_CHANNELS];                
                ma_libopus_ds_get_data_format(first, &pPCMDataSource->format, &pPCMDataSource->channels, &pPCMDataSource->sampleRate, channelMap, MA_MAX_CHANNELS);                
                ma_data_source_get_length_in_pcm_frames(first, &pPCMDataSource->totalFrames);
                ma_data_source_base *base = (ma_data_source_base*)first;
                base->pCurrent = first;
                first->pReadSeekTellUserData = pPCMDataSource;                
        }
        else if (endsWith(filePath, "ogg"))
        {
                prepareNextVorbisDecoder(filePath);
                ma_libvorbis *first = getFirstVorbisDecoder();
                ma_channel channelMap[MA_MAX_CHANNELS];              
                ma_libvorbis_ds_get_data_format(first, &pPCMDataSource->format, &pPCMDataSource->channels, &pPCMDataSource->sampleRate, channelMap, MA_MAX_CHANNELS);
                ma_data_source_get_length_in_pcm_frames(first, &pPCMDataSource->totalFrames);
                ma_data_source_base *base = (ma_data_source_base*)first;
                base->pCurrent = first;                
                first->pReadSeekTellUserData = pPCMDataSource;                
        }
        else
        {
                if (pPCMDataSource->fileA == NULL)
                {
                        pPCMDataSource->filenameA = pUserData->filenameA;
                        pPCMDataSource->fileA = fopen(pUserData->filenameA, "rb");
                }

                pPCMDataSource->format = SAMPLE_FORMAT;
                pPCMDataSource->channels = CHANNELS;
                pPCMDataSource->sampleRate = SAMPLE_RATE;
        }

        return MA_SUCCESS;
}

void createDevice(UserData *userData, ma_device *device, ma_context *context, ma_data_source_vtable *vtable, ma_device_data_proc callback)
{
        ma_result result;

        ma_data_source_uninit(&pcmDataSource);
        initFirstDatasource(&pcmDataSource, userData);

        pcmDataSource.base.vtable = vtable;

        ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
        deviceConfig.playback.format = pcmDataSource.format;
        deviceConfig.playback.channels = pcmDataSource.channels;
        deviceConfig.sampleRate = pcmDataSource.sampleRate;
        deviceConfig.dataCallback = callback;
        deviceConfig.pUserData = &pcmDataSource;

        result = ma_device_init(context, &deviceConfig, device);
        if (result != MA_SUCCESS)
                return;
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

        initFirstDatasource(&pcmDataSource, userData);
        ma_libvorbis *vorbis = getFirstVorbisDecoder();
        ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);

        deviceConfig.playback.format = vorbis->format;
        deviceConfig.playback.channels = pcmDataSource.channels;
        deviceConfig.sampleRate = pcmDataSource.sampleRate;
        deviceConfig.dataCallback = vorbis_on_audio_frames;
        deviceConfig.pUserData = vorbis;

        result = ma_device_init(context, &deviceConfig, device);
        if (result != MA_SUCCESS)
        {
                printf("Failed to initialize miniaudio device.\n");
                return;
        }

        // Start the device again
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

        initFirstDatasource(&pcmDataSource, userData);
        ma_libopus *opus = getFirstOpusDecoder();  

        ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);

        deviceConfig.playback.format = opus->format;
        deviceConfig.playback.channels = pcmDataSource.channels;
        deviceConfig.sampleRate = pcmDataSource.sampleRate;
        deviceConfig.dataCallback = opus_on_audio_frames;
        deviceConfig.pUserData = opus;        

        result = ma_device_init(context, &deviceConfig, device);
        if (result != MA_SUCCESS)
        {
                printf("Failed to initialize miniaudio device.\n");
                return;
        }

        // Start the device again
        result = ma_device_start(device);
        if (result != MA_SUCCESS)
        {
                printf("Failed to start miniaudio device.\n");
                return;
        }
}

void switchAudioImplementation()
{
        enum AudioImplementation currentImplementation = getCurrentImplementationType();

        if (g_userData->currentSongData == NULL)
                return;

        char *filePath = strdup(g_userData->currentSongData->filePath);

        if (filePath == NULL || filePath[0] == '\0' || filePath[0] == '\r')
        {
                free(filePath);
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
                        
                        resetDecoders();
                        resetVorbisDecoders();
                        resetOpusDecoders();
                        resetAudioBuffer();

                        cleanupPlaybackDevice();

                        builtin_createAudioDevice(g_userData, getDevice(), &context, &builtin_file_data_source_vtable);

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

                if (!isRepeatEnabled() && (sameFormat && currentImplementation == OPUS))
                {
                        ma_data_source_get_length_in_pcm_frames(getCurrentOpusDecoder(), &pcmDataSource.totalFrames);
                }
                else
                {
                        setImplSwitchReached();

                        pthread_mutex_lock(&dataSourceMutex);

                        setCurrentImplementationType(OPUS); 

                        resetDecoders();
                        resetVorbisDecoders();
                        resetOpusDecoders();
                        resetAudioBuffer();

                        cleanupPlaybackDevice();

                        opus_createAudioDevice(g_userData, getDevice(), &context, &pcm_file_data_source_vtable);

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

                if (!isRepeatEnabled() && (sameFormat && currentImplementation == VORBIS))
                {
                        ma_data_source_get_length_in_pcm_frames(getCurrentVorbisDecoder(), &pcmDataSource.totalFrames);
                }
                else
                {
                        setImplSwitchReached();
                        
                        pthread_mutex_lock(&dataSourceMutex);

                        setCurrentImplementationType(VORBIS);

                        resetDecoders();
                        resetVorbisDecoders();
                        resetOpusDecoders();
                        resetAudioBuffer();

                        cleanupPlaybackDevice(); 
                                           
                        vorbis_createAudioDevice(g_userData, getDevice(), &context, &pcm_file_data_source_vtable);

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
                        resetDecoders();
                        resetVorbisDecoders();
                        resetOpusDecoders();
                        resetAudioBuffer();
                        cleanupPlaybackDevice();                       
                        pcm_createAudioDevice(g_userData, getDevice(), &context, &pcm_file_data_source_vtable);

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
        g_userData = userData;
        ma_context_init(NULL, 0, NULL, &context);
        switchAudioImplementation();
}