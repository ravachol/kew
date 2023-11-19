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

ma_result pcm_file_data_source_init(PCMFileDataSource *pPCMDataSource, UserData *pUserData)
{
        char *filePath = NULL;

        filePath = (pPCMDataSource->currentFileIndex == 0) ? pUserData->songdataA->filePath : pUserData->songdataB->filePath;

        if (hasBuiltinDecoder(filePath))
        {
                prepareNextDecoder(filePath);
                ma_decoder *first = getFirstDecoder();
                pPCMDataSource->format = first->outputFormat;
                pPCMDataSource->channels = first->outputChannels;
                pPCMDataSource->sampleRate = first->outputSampleRate;

                ma_data_source_get_length_in_pcm_frames(first, &pPCMDataSource->totalFrames);
        }
        else
        {
                char *filePath = NULL;

                if (pPCMDataSource->fileA == NULL)
                {
                        pPCMDataSource->filenameA = pUserData->filenameA;
                        pPCMDataSource->fileA = fopen(pUserData->filenameA, "rb");
                }

                pPCMDataSource->format = SAMPLE_FORMAT;
                pPCMDataSource->channels = CHANNELS;
                pPCMDataSource->sampleRate = SAMPLE_RATE;
        }

        pPCMDataSource->pUserData = pUserData;
        pPCMDataSource->currentPCMFrame = 0;

        return MA_SUCCESS;
}

void createDevice(UserData *userData, ma_device *device, ma_context *context, ma_data_source_vtable *vtable, ma_device_data_proc callback)
{
        ma_result result;

        if (result != MA_SUCCESS)
                return;

        ma_data_source_uninit(&pcmDataSource);
        pcm_file_data_source_init(&pcmDataSource, userData);

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

void switchAudioImplementation()
{
        enum AudioImplementation currentImplementation = getCurrentImplementationType();
        char *filePath = g_userData->currentSongData->filePath;

        if (hasBuiltinDecoder(filePath))
        {
                ma_uint32 sampleRate;
                ma_uint32 channels;
                ma_format format;
                ma_decoder *decoder = getCurrentDecoder();

                getFileInfo(filePath, &sampleRate, &channels, &format);

                bool sameFormat = (decoder == NULL || (sampleRate == decoder->outputSampleRate &&
                                                       channels == decoder->outputChannels &&
                                                       format == decoder->outputFormat));

                if (sameFormat && currentImplementation == BUILTIN)
                {
                        setEOFNotReached();
                        setCurrentImplementationType(BUILTIN);
                        return;
                }
                else
                {                        
                        cleanupPlaybackDevice();
                        resetDecoders();
                        builtin_createAudioDevice(g_userData, getDevice(), &context, &builtin_file_data_source_vtable);
                        setCurrentImplementationType(BUILTIN);
                }
        }
        else
        {
                if (currentImplementation == PCM)
                {
                        setEOFNotReached();
                        setCurrentImplementationType(PCM);
                        return;
                }                
                cleanupPlaybackDevice();
                resetDecoders();
                pcm_createAudioDevice(g_userData, getDevice(), &context, &pcm_file_data_source_vtable);
                setCurrentImplementationType(PCM);
        }

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