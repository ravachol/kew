#include <stdint.h>
#define MA_EXPERIMENTAL__DATA_LOOPING_AND_CHAINING
#define MA_NO_ENGINE
#define MINIAUDIO_IMPLEMENTATION

#include "appstate.h"
#include "common.h"
#include "file.h"
#include "sound.h"
#include "soundbuiltin.h"
#include "utils.h"
#include <miniaudio.h>
#include <stdio.h>
#include <stdlib.h>
#include "soundcommon.h"

/*

sound.c

 Functions related to miniaudio implementation

*/

static ma_context context;
static bool contextInitialized = false;
static bool tryAgain = false;

UserData userData;

UserData *getUserData() { return &userData; }

bool isContextInitialized() { return contextInitialized; }

ma_result initFirstDatasource(UserData **pUserData)
{
        char *filePath = NULL;

        AudioData *audioData = getAudioData();

        SongData *songData = (audioData->currentFileIndex == 0)
                                 ? (*pUserData)->songdataA
                                 : (*pUserData)->songdataB;

        if (songData == NULL)
        {
                return MA_ERROR;
        }

        filePath = songData->filePath;

        if (filePath == NULL)
        {
                return MA_ERROR;
        }

        audioData->pUserData = *pUserData;
        audioData->currentPCMFrame = 0;
        audioData->restart = false;

        if (hasBuiltinDecoder(filePath))
        {
                int result = prepareNextDecoder(filePath);

                if (result < 0)
                        return -1;

                ma_decoder *first = getFirstDecoder();
                audioData->format = first->outputFormat;
                audioData->channels = first->outputChannels;
                audioData->sampleRate = first->outputSampleRate;

                ma_data_source_get_length_in_pcm_frames(
                    first, &(audioData->totalFrames));
        }
        else if (pathEndsWith(filePath, "opus"))
        {
                int result = prepareNextOpusDecoder(filePath);

                if (result < 0)
                        return -1;

                ma_libopus *first = getFirstOpusDecoder();
                ma_channel channelMap[MA_MAX_CHANNELS];

                ma_libopus_ds_get_data_format(
                    first, &(audioData->format), &(audioData->channels),
                    &(audioData->sampleRate), channelMap, MA_MAX_CHANNELS);

                ma_data_source_get_length_in_pcm_frames(
                    first, &(audioData->totalFrames));

                ma_data_source_base *base = (ma_data_source_base *)first;

                base->pCurrent = first;
                first->pReadSeekTellUserData = audioData;
        }
        else if (pathEndsWith(filePath, "ogg"))
        {
                int result = prepareNextVorbisDecoder(filePath);
                if (result < 0)
                        return -1;
                ma_libvorbis *first = getFirstVorbisDecoder();
                ma_channel channelMap[MA_MAX_CHANNELS];
                ma_libvorbis_ds_get_data_format(
                    first, &(audioData->format), &(audioData->channels),
                    &(audioData->sampleRate), channelMap, MA_MAX_CHANNELS);
                ma_data_source_get_length_in_pcm_frames(
                    first, &(audioData->totalFrames));
                ma_data_source_base *base = (ma_data_source_base *)first;
                base->pCurrent = first;
                first->pReadSeekTellUserData = audioData;
        }
        else if (pathEndsWith(filePath, "webm"))
        {
                int result = prepareNextWebmDecoder(songData);
                if (result < 0)
                        return -1;
                ma_webm *first = getFirstWebmDecoder();
                ma_channel channelMap[MA_MAX_CHANNELS];
                ma_webm_ds_get_data_format(
                    first, &(audioData->format), &(audioData->channels),
                    &(audioData->sampleRate), channelMap, MA_MAX_CHANNELS);
                ma_data_source_get_length_in_pcm_frames(
                    first, &(audioData->totalFrames));
                ma_data_source_base *base = (ma_data_source_base *)first;
                base->pCurrent = first;
                first->pReadSeekTellUserData = audioData;
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
                    first, &(audioData->format), &(audioData->channels),
                    &(audioData->sampleRate), channelMap, MA_MAX_CHANNELS);
                ma_data_source_get_length_in_pcm_frames(
                    first, &(audioData->totalFrames));
                ma_data_source_base *base = (ma_data_source_base *)first;
                base->pCurrent = first;
                first->pReadSeekTellUserData = audioData;
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

int createDevice(AppState *state, UserData *userData, ma_device *device, ma_context *context,
                 ma_data_source_vtable *vtable, ma_device_data_proc callback)
{
        ma_result result;

        result = initFirstDatasource(&userData);

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

        state->uiState.doNotifyMPRISPlaying = true;

        return 0;
}

int builtin_createAudioDevice(AppState *state, UserData *userData, ma_device *device,
                              ma_context *context,
                              ma_data_source_vtable *vtable)
{
        return createDevice(state, userData, device, context, vtable,
                            builtin_on_audio_frames);
}

int vorbis_createAudioDevice(AppState *state, UserData *userData, ma_device *device,
                             ma_context *context)
{
        ma_result result = initFirstDatasource(&userData);

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

        state->uiState.doNotifyMPRISPlaying = true;

        return 0;
}

#ifdef USE_FAAD
int m4a_createAudioDevice(AppState *state, UserData *userData, ma_device *device,
                          ma_context *context)
{
        ma_result result = initFirstDatasource(&userData);

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

        state->uiState.doNotifyMPRISPlaying = true;

        return 0;
}
#endif

int opus_createAudioDevice(AppState *state, UserData *userData, ma_device *device,
                           ma_context *context)
{
        ma_result result;

        result = initFirstDatasource(&userData);

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

        state->uiState.doNotifyMPRISPlaying = true;

        return 0;
}

int webm_createAudioDevice(AppState *state, UserData *userData, ma_device *device,
                           ma_context *context)
{
        ma_result result;

        result = initFirstDatasource(&userData);
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

        state->uiState.doNotifyMPRISPlaying = true;

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

int switchAudioImplementation(AppState *state)
{
        AudioData *audioData = getAudioData();

        if (audioData->endOfListReached)
        {
                setEOFNotReached();
                setCurrentImplementationType(NONE);
                return 0;
        }

        enum AudioImplementation currentImplementation =
            getCurrentImplementationType();

        userData.currentSongData = (audioData->currentFileIndex == 0)
                                       ? userData.songdataA
                                       : userData.songdataB;

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
                                AudioData *audioData = getAudioData();

                                int idx = (audioData != NULL) ? audioData->currentFileIndex : 0;

                                setCurrentFileIndex(audioData, 1 - idx);

                                tryAgain = true;

                                switchAudioImplementation(state);

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
                            audioData->avgBitRate = avgBitRate;
                }
                else
                        audioData->avgBitRate = 0;

                if (isRepeatEnabled() ||
                    !(sameFormat && currentImplementation == BUILTIN))
                {
                        setImplSwitchReached();

                        pthread_mutex_lock(&(state->dataSourceMutex));

                        setCurrentImplementationType(BUILTIN);

                        cleanupPlaybackDevice();

                        resetAllDecoders();
                        resetAudioBuffer();

                        audioData->sampleRate = sampleRate;

                        int result = builtin_createAudioDevice(state,
                                                               &userData, getDevice(), &context,
                                                               &builtin_file_data_source_vtable);

                        if (result < 0)
                        {
                                setCurrentImplementationType(NONE);
                                setImplSwitchNotReached();
                                setEOFReached();
                                free(filePath);
                                pthread_mutex_unlock(&(state->dataSourceMutex));
                                return -1;
                        }

                        pthread_mutex_unlock(&(state->dataSourceMutex));

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

                        pthread_mutex_lock(&(state->dataSourceMutex));

                        setCurrentImplementationType(OPUS);

                        cleanupPlaybackDevice();

                        resetAllDecoders();
                        resetAudioBuffer();

                        audioData->sampleRate = sampleRate;
                        audioData->avgBitRate = 0;

                        int result = opus_createAudioDevice(state,
                                                            &userData, getDevice(), &context);

                        if (result < 0)
                        {
                                setCurrentImplementationType(NONE);
                                setImplSwitchNotReached();
                                setEOFReached();
                                free(filePath);
                                pthread_mutex_unlock(&(state->dataSourceMutex));
                                return -1;
                        }

                        pthread_mutex_unlock(&(state->dataSourceMutex));

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
                            audioData->avgBitRate = calcAvgBitRate(
                                userData.currentSongData->duration, filePath);
                else
                        audioData->avgBitRate = 0;

                if (isRepeatEnabled() ||
                    !(sameFormat && currentImplementation == VORBIS))
                {
                        setImplSwitchReached();

                        pthread_mutex_lock(&(state->dataSourceMutex));

                        setCurrentImplementationType(VORBIS);

                        cleanupPlaybackDevice();

                        resetAllDecoders();
                        resetAudioBuffer();

                        audioData->sampleRate = sampleRate;

                        int result = vorbis_createAudioDevice(state,
                                                              &userData, getDevice(), &context);

                        if (result < 0)
                        {
                                setCurrentImplementationType(NONE);
                                setImplSwitchNotReached();
                                setEOFReached();
                                free(filePath);
                                pthread_mutex_unlock(&(state->dataSourceMutex));
                                return -1;
                        }

                        pthread_mutex_unlock(&(state->dataSourceMutex));

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

                audioData->avgBitRate = 0;

                if (isRepeatEnabled() ||
                    !(sameFormat && currentImplementation == WEBM))
                {
                        setImplSwitchReached();

                        pthread_mutex_lock(&(state->dataSourceMutex));

                        setCurrentImplementationType(WEBM);

                        cleanupPlaybackDevice();

                        resetAllDecoders();
                        resetAudioBuffer();

                        audioData->sampleRate = sampleRate;

                        int result = webm_createAudioDevice(state,
                                                            &userData, getDevice(), &context);

                        if (result < 0)
                        {
                                setCurrentImplementationType(NONE);
                                setImplSwitchNotReached();
                                setEOFReached();
                                free(filePath);
                                pthread_mutex_unlock(&(state->dataSourceMutex));
                                return -1;
                        }

                        pthread_mutex_unlock(&(state->dataSourceMutex));

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
                            audioData->avgBitRate = avgBitRate;

                if (isRepeatEnabled() ||
                    !(sameFormat && currentImplementation == M4A))
                {
                        setImplSwitchReached();

                        pthread_mutex_lock(&(state->dataSourceMutex));

                        setCurrentImplementationType(M4A);

                        cleanupPlaybackDevice();

                        resetAllDecoders();
                        resetAudioBuffer();

                        audioData->sampleRate = sampleRate;

                        int result = m4a_createAudioDevice(state,
                                                           &userData, getDevice(), &context);

                        if (result < 0)
                        {
                                setCurrentImplementationType(NONE);
                                setImplSwitchNotReached();
                                setEOFReached();
                                free(filePath);
                                pthread_mutex_unlock(&(state->dataSourceMutex));
                                return -1;
                        }

                        pthread_mutex_unlock(&(state->dataSourceMutex));

                        setImplSwitchNotReached();
                }
#else
                setCurrentImplementationType(NONE);
                setImplSwitchNotReached();
                setEOFReached();
                free(filePath);
                pthread_mutex_unlock(&state->dataSourceMutex);
                setErrorMessage("Can't load m4a files. Faad library is needed at compile time.");
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
        contextInitialized = false;
}

int createAudioDevice(AppState *state)
{
        if (contextInitialized)
        {
                ma_context_uninit(&context);
                contextInitialized = false;
        }
        ma_context_init(NULL, 0, NULL, &context);
        contextInitialized = true;

        if (switchAudioImplementation(state) >= 0)
        {
                state->uiState.doNotifyMPRISSwitched = true;
        }
        else
        {
                return -1;
        }

        return 0;
}
