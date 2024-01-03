#include "soundcommon.h"

#define MAX_DECODERS 2

/*

soundcommon.c

 Related to common functions for decoders / miniaudio implementations

*/

bool repeatEnabled = false;
bool shuffleEnabled = false;
bool skipToNext = false;
bool seekRequested = false;
bool paused = false;
float seekPercent = 0.0;
double seekElapsed;
_Atomic bool EOFReached = false;
_Atomic bool switchReached = false;
_Atomic bool readingFrames = false;
pthread_mutex_t dataSourceMutex = PTHREAD_MUTEX_INITIALIZER;
ma_device device = {0};
ma_int32 *audioBuffer = NULL;
ma_decoder *firstDecoder;
ma_decoder *currentDecoder;
int bufSize;
ma_event switchAudioImpl;
enum AudioImplementation currentImplementation = NONE;
ma_decoder *decoders[MAX_DECODERS];
ma_libopus *opusDecoders[MAX_DECODERS];
ma_libopus *firstOpusDecoder;
ma_libvorbis *vorbisDecoders[MAX_DECODERS];
ma_libvorbis *firstVorbisDecoder;
int decoderIndex = -1;
int opusDecoderIndex = -1;
int vorbisDecoderIndex = -1;
bool doQuit = false;

enum AudioImplementation getCurrentImplementationType()
{
        return currentImplementation;
}

void setCurrentImplementationType(enum AudioImplementation value)
{
        currentImplementation = value;
}

ma_decoder *getFirstDecoder()
{
        return firstDecoder;
}

ma_decoder *getCurrentDecoder()
{
        if (decoderIndex == -1)
                return getFirstDecoder();
        else
                return decoders[decoderIndex];
}

void switchDecoder()
{
        if (decoderIndex == -1)
                decoderIndex = 0;
        else
                decoderIndex = 1 - decoderIndex;
}

void setNextDecoder(ma_decoder *decoder)
{
        if (decoderIndex == -1 && firstDecoder == NULL)
        {
                firstDecoder = decoder;
        }
        else if (decoderIndex == -1)
        {
                decoders[0] = decoder;
        }
        else
        {
                int nextIndex = 1 - decoderIndex;
                decoders[nextIndex] = decoder;
        }
}

void resetDecoders()
{
        decoderIndex = -1;

        if (firstDecoder != NULL && firstDecoder->outputFormat != ma_format_unknown)
        {
                ma_decoder_uninit(firstDecoder);
                firstDecoder = NULL;
        }

        if (decoders[0] != NULL && decoders[0]->outputFormat != ma_format_unknown)
        {
                ma_decoder_uninit(decoders[0]);
                decoders[0] = NULL;
        }

        if (decoders[1] != NULL && decoders[1]->outputFormat != ma_format_unknown)
        {
                ma_decoder_uninit(decoders[1]);
                decoders[1] = NULL;
        }
}

void uninitPreviousDecoder()
{
        if (decoderIndex == -1)
        {
                return;
        }
        ma_decoder *toUninit = decoders[1 - decoderIndex];

        if (toUninit != NULL)
        {
                ma_decoder_uninit(toUninit);
                free(toUninit);
                decoders[1 - decoderIndex] = NULL;
        }
}

void uninitPreviousVorbisDecoder()
{
        if (vorbisDecoderIndex == -1)
        {
                return;
        }
        ma_libvorbis *toUninit = vorbisDecoders[1 - vorbisDecoderIndex];

        if (toUninit != NULL)
        {
                ma_libvorbis_uninit(toUninit, NULL);
                free(toUninit);
                vorbisDecoders[1 - vorbisDecoderIndex] = NULL;
        }
}

void uninitPreviousOpusDecoder()
{
        if (opusDecoderIndex == -1)
        {
                return;
        }
        ma_libopus *toUninit = opusDecoders[1 - opusDecoderIndex];

        if (toUninit != NULL)
        {
                ma_libopus_uninit(toUninit, NULL);
                free(toUninit);
                opusDecoders[1 - opusDecoderIndex] = NULL;
        }
}

ma_libvorbis *getFirstVorbisDecoder()
{
        return firstVorbisDecoder;
}

ma_libvorbis *getCurrentVorbisDecoder()
{
        if (vorbisDecoderIndex == -1)
                return getFirstVorbisDecoder();
        else
                return vorbisDecoders[vorbisDecoderIndex];
}

void switchVorbisecoder()
{
        if (vorbisDecoderIndex == -1)
                vorbisDecoderIndex = 0;
        else
                vorbisDecoderIndex = 1 - vorbisDecoderIndex;
}

ma_libopus *getFirstOpusDecoder()
{
        return firstOpusDecoder;
}

ma_libopus *getCurrentOpusDecoder()
{
        if (opusDecoderIndex == -1)
                return getFirstOpusDecoder();
        else
                return opusDecoders[opusDecoderIndex];
}

void switchVorbisDecoder()
{
        if (vorbisDecoderIndex == -1)
                vorbisDecoderIndex = 0;
        else
                vorbisDecoderIndex = 1 - vorbisDecoderIndex;
}

void switchOpusDecoder()
{
        if (opusDecoderIndex == -1)
                opusDecoderIndex = 0;
        else
                opusDecoderIndex = 1 - opusDecoderIndex;
}

void setNextVorbisDecoder(ma_libvorbis *decoder)
{
        if (vorbisDecoderIndex == -1 && firstVorbisDecoder == NULL)
        {
                firstVorbisDecoder = decoder;
        }
        else if (vorbisDecoderIndex == -1)
        {
                vorbisDecoders[0] = decoder;
        }
        else
        {
                int nextIndex = 1 - vorbisDecoderIndex;
                vorbisDecoders[nextIndex] = decoder;
        }
}

void setNextOpusDecoder(ma_libopus *decoder)
{
        if (opusDecoderIndex == -1 && firstOpusDecoder == NULL)
        {
                firstOpusDecoder = decoder;
        }
        else if (opusDecoderIndex == -1)
        {
                opusDecoders[0] = decoder;
        }
        else
        {
                int nextIndex = 1 - opusDecoderIndex;
                opusDecoders[nextIndex] = decoder;
        }
}

void resetVorbisDecoders()
{
        vorbisDecoderIndex = -1;

        if (firstVorbisDecoder != NULL && firstVorbisDecoder->format != ma_format_unknown)
        {
                ma_libvorbis_uninit(firstVorbisDecoder, NULL);
                free(firstVorbisDecoder);
                firstVorbisDecoder = NULL;
        }

        if (vorbisDecoders[0] != NULL && vorbisDecoders[0]->format != ma_format_unknown)
        {
                ma_libvorbis_uninit(vorbisDecoders[0], NULL);
                free(vorbisDecoders[0]);
                vorbisDecoders[0] = NULL;
        }

        if (vorbisDecoders[1] != NULL && vorbisDecoders[1]->format != ma_format_unknown)
        {
                ma_libvorbis_uninit(vorbisDecoders[1], NULL);
                free(vorbisDecoders[1]);
                vorbisDecoders[1] = NULL;
        }
}

void resetOpusDecoders()
{
        opusDecoderIndex = -1;

        if (firstOpusDecoder != NULL && firstOpusDecoder->format != ma_format_unknown)
        {
                ma_data_source_base *base = (ma_data_source_base *)firstOpusDecoder;
                base->pCurrent = NULL;
                base->vtable = NULL;
                ma_libopus_uninit(firstOpusDecoder, NULL);
                free(firstOpusDecoder);
                firstOpusDecoder = NULL;
        }

        if (opusDecoders[0] != NULL && opusDecoders[0]->format != ma_format_unknown)
        {
                ma_libopus_uninit(opusDecoders[0], NULL);
                free(opusDecoders[0]);
                opusDecoders[0] = NULL;
        }

        if (opusDecoders[1] != NULL && opusDecoders[1]->format != ma_format_unknown)
        {
                ma_libopus_uninit(opusDecoders[1], NULL);
                free(opusDecoders[1]);
                opusDecoders[1] = NULL;
        }
}

void prepareNextVorbisDecoder(char *filepath)
{
        ma_libvorbis *currentDecoder;

        if (vorbisDecoderIndex == -1)
        {
                currentDecoder = getFirstVorbisDecoder();
        }
        else
        {
                currentDecoder = vorbisDecoders[vorbisDecoderIndex];
        }

        ma_uint32 sampleRate;
        ma_uint32 channels;
        ma_format format;
        ma_channel channelMap[MA_MAX_CHANNELS];
        ma_libvorbis_get_data_format(currentDecoder, &format, &channels, &sampleRate, channelMap, MA_MAX_CHANNELS);

        uninitPreviousVorbisDecoder();

        ma_libvorbis *decoder = (ma_libvorbis *)malloc(sizeof(ma_libvorbis));
        ma_libvorbis_init_file(filepath, NULL, NULL, decoder);

        ma_format nformat;
        ma_uint32 nchannels;
        ma_uint32 nsampleRate;
        ma_channel nchannelMap[MA_MAX_CHANNELS];

        ma_libvorbis_get_data_format(decoder, &nformat, &nchannels, &nsampleRate, nchannelMap, MA_MAX_CHANNELS);
        bool sameFormat = (currentDecoder == NULL || (format == nformat &&
                                                      channels == nchannels &&
                                                      sampleRate == nsampleRate));

        if (!sameFormat)
        {
                ma_libvorbis_uninit(decoder, NULL);
                free(decoder);
                return;
        }

        ma_libvorbis *first = getFirstVorbisDecoder();
        if (first != NULL)
        {
                decoder->pReadSeekTellUserData = (AudioData *)first->pReadSeekTellUserData;
        }

        decoder->format = nformat;
        decoder->onRead = ma_libvorbis_read_pcm_frames_wrapper;
        decoder->onSeek = ma_libvorbis_seek_to_pcm_frame_wrapper;
        decoder->onTell = ma_libvorbis_get_cursor_in_pcm_frames_wrapper;

        setNextVorbisDecoder(decoder);
        if (currentDecoder != NULL)
                ma_data_source_set_next(currentDecoder, decoder);
}

void prepareNextOpusDecoder(char *filepath)
{
        ma_libopus *currentDecoder;

        if (opusDecoderIndex == -1)
        {
                currentDecoder = getFirstOpusDecoder();
        }
        else
        {
                currentDecoder = opusDecoders[decoderIndex];
        }

        ma_uint32 sampleRate;
        ma_uint32 channels;
        ma_format format;
        ma_channel channelMap[MA_MAX_CHANNELS];
        ma_libopus_get_data_format(currentDecoder, &format, &channels, &sampleRate, channelMap, MA_MAX_CHANNELS);

        uninitPreviousOpusDecoder();

        ma_libopus *decoder = (ma_libopus *)malloc(sizeof(ma_libopus));
        ma_libopus_init_file(filepath, NULL, NULL, decoder);

        ma_format nformat;
        ma_uint32 nchannels;
        ma_uint32 nsampleRate;
        ma_channel nchannelMap[MA_MAX_CHANNELS];

        ma_libopus_get_data_format(decoder, &nformat, &nchannels, &nsampleRate, nchannelMap, MA_MAX_CHANNELS);
        bool sameFormat = (currentDecoder == NULL || (format == nformat &&
                                                      channels == nchannels &&
                                                      sampleRate == nsampleRate));

        if (!sameFormat)
        {
                ma_libopus_uninit(decoder, NULL);
                free(decoder);
                return;
        }

        ma_libopus *first = getFirstOpusDecoder();
        if (first != NULL)
        {
                decoder->pReadSeekTellUserData = (AudioData *)first->pReadSeekTellUserData;
        }

        decoder->format = nformat;
        decoder->onRead = ma_libopus_read_pcm_frames_wrapper;
        decoder->onSeek = ma_libopus_seek_to_pcm_frame_wrapper;
        decoder->onTell = ma_libopus_get_cursor_in_pcm_frames_wrapper;

        setNextOpusDecoder(decoder);
        if (currentDecoder != NULL)
                ma_data_source_set_next(currentDecoder, decoder);
}

void getFileInfo(const char *filename, ma_uint32 *sampleRate, ma_uint32 *channels, ma_format *format)
{
        ma_decoder tmp;
        if (ma_decoder_init_file(filename, NULL, &tmp) == MA_SUCCESS)
        {
                *sampleRate = tmp.outputSampleRate;
                *channels = tmp.outputChannels;
                *format = tmp.outputFormat;
                ma_decoder_uninit(&tmp);
        }
        else
        {
                // Handle file open error.
        }
}

int prepareNextDecoder(char *filepath)
{
        ma_decoder *currentDecoder;

        if (decoderIndex == -1)
        {
                currentDecoder = getFirstDecoder();
        }
        else
        {
                currentDecoder = decoders[decoderIndex];
        }

        ma_uint32 sampleRate;
        ma_uint32 channels;
        ma_format format;
        getFileInfo(filepath, &sampleRate, &channels, &format);

        bool sameFormat = (currentDecoder == NULL || (format == currentDecoder->outputFormat &&
                                                      channels == currentDecoder->outputChannels &&
                                                      sampleRate == currentDecoder->outputSampleRate));

        if (!sameFormat)
        {
                return 0;
        }

        uninitPreviousDecoder();

        ma_decoder *decoder = (ma_decoder *)malloc(sizeof(ma_decoder));
        ma_result result = ma_decoder_init_file(filepath, NULL, decoder);

        if (result != MA_SUCCESS)
                return -1;

        setNextDecoder(decoder);
        if (currentDecoder != NULL)
                ma_data_source_set_next(currentDecoder, decoder);

        return 0;
}

void getVorbisFileInfo(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sampleRate, ma_channel *channelMap)
{
        ma_libvorbis decoder;
        if (ma_libvorbis_init_file(filename, NULL, NULL, &decoder) == MA_SUCCESS)
        {
                *format = decoder.format;
                ma_libvorbis_get_data_format(&decoder, format, channels, sampleRate, channelMap, MA_MAX_CHANNELS);
                ma_libvorbis_uninit(&decoder, NULL);
        }
}

void getOpusFileInfo(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sampleRate, ma_channel *channelMap)
{
        ma_libopus decoder;

        if (ma_libopus_init_file(filename, NULL, NULL, &decoder) == MA_SUCCESS)
        {
                *format = decoder.format;
                ma_libopus_get_data_format(&decoder, format, channels, sampleRate, channelMap, MA_MAX_CHANNELS);
                ma_libopus_uninit(&decoder, NULL);
        }
}

int getBufferSize()
{
        return bufSize;
}

void setBufferSize(int value)
{
        bufSize = value;
}

void initAudioBuffer()
{
        if (audioBuffer == NULL)
        {
                audioBuffer = malloc(sizeof(ma_int32) * MAX_BUFFER_SIZE);
                if (audioBuffer == NULL)
                {
                        // Memory allocation failed
                        return;
                }
        }
}

ma_int32 *getAudioBuffer()
{
        return audioBuffer;
}

void setAudioBuffer(ma_int32 *buf)
{
        audioBuffer = buf;
}

void resetAudioBuffer()
{
        memset(audioBuffer, 0, sizeof(float) * MAX_BUFFER_SIZE);
}

void freeAudioBuffer()
{
        if (audioBuffer != NULL)
        {
                free(audioBuffer);
                audioBuffer = NULL;
        }
}

bool isRepeatEnabled()
{
        return repeatEnabled;
}

void setRepeatEnabled(bool value)
{
        repeatEnabled = value;
}

bool isShuffleEnabled()
{
        return shuffleEnabled;
}

void setShuffleEnabled(bool value)
{
        shuffleEnabled = value;
}

bool isSkipToNext()
{
        return skipToNext;
}

void setSkipToNext(bool value)
{
        skipToNext = value;
}

double getSeekElapsed()
{
        return seekElapsed;
}

double getPercentageElapsed()
{
        return elapsedSeconds / duration;
}

void setSeekElapsed(double value)
{
        seekElapsed = value;
}

bool isEOFReached()
{
        return atomic_load(&EOFReached);
}

void setEOFReached()
{
        atomic_store(&EOFReached, true);
}

void setEOFNotReached()
{
        atomic_store(&EOFReached, false);
}

bool isImplSwitchReached()
{
        return atomic_load(&switchReached) ? true : false;
}

void setImplSwitchReached()
{
        atomic_store(&switchReached, true);
}

void setImplSwitchNotReached()
{
        atomic_store(&switchReached, false);
}

bool isPlaying()
{
        return ma_device_is_started(&device);
}

bool isPlaybackDone()
{
        if (isEOFReached())
        {
                return true;
        }
        else
        {
                return false;
        }
}

float getSeekPercentage()
{
        return seekPercent;
}

bool isSeekRequested()
{
        return seekRequested;
}

void setSeekRequested(bool value)
{
        seekRequested = value;
}

void seekPercentage(float percent)
{
        seekPercent = percent;
        seekRequested = true;
}

void resumePlayback()
{
        if (!ma_device_is_started(&device))
        {
                ma_device_start(&device);
        }

        paused = false;

        if (appState.currentView != SONG_VIEW)
        {
                refresh = true;
        }
}

void pausePlayback()
{
        if (ma_device_is_started(&device))
        {
                ma_device_stop(&device);
        }

        paused = true;

        if (appState.currentView != SONG_VIEW)
        {
                refresh = true;
        }
}

void cleanupPlaybackDevice()
{
        ma_device_stop(&device);
        while (ma_device_get_state(&device) == ma_device_state_started)
        {
                c_sleep(100);
        }
        ma_device_uninit(&device);
}

void togglePausePlayback()
{
        if (ma_device_is_started(&device))
        {
                pausePlayback();
        }
        else if (paused)
        {
                resumePlayback();
        }
}

bool isPaused()
{
        return paused;
}

pthread_mutex_t deviceMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t deviceStopped = PTHREAD_COND_INITIALIZER;

void resetDevice()
{
        pthread_mutex_lock(&deviceMutex);

        if (ma_device_get_state(&device) == ma_device_state_started)
                ma_device_stop(&device);

        while (ma_device_get_state(&device) == ma_device_state_started)
        {
                pthread_cond_wait(&deviceStopped, &deviceMutex);
        }

        pthread_mutex_unlock(&deviceMutex);

        ma_device_uninit(&device);
}

ma_device *getDevice()
{
        return &device;
}

bool hasBuiltinDecoder(char *filePath)
{
        char *extension = strrchr(filePath, '.');
        return (extension != NULL && (strcasecmp(extension, ".wav") == 0 || strcasecmp(extension, ".flac") == 0 ||
                                      strcasecmp(extension, ".mp3") == 0));
}

void activateSwitch(AudioData *pAudioData)
{
        setSkipToNext(false);
        if (!isRepeatEnabled())
                pAudioData->currentFileIndex = 1 - pAudioData->currentFileIndex; // Toggle between 0 and 1
        pAudioData->switchFiles = true;
}

void executeSwitch(AudioData *pAudioData)
{
        pAudioData->switchFiles = false;
        switchDecoder();
        switchOpusDecoder();
        switchVorbisDecoder();

        pAudioData->totalFrames = 0;

        // Close the current file, and open the new one
        char *currentFilename;
        SongData *currentSongData;

        if (pAudioData->currentFileIndex == 0)
        {
                if (pAudioData->fileB != NULL)
                        fclose(pAudioData->fileB);
                pAudioData->fileB = NULL;
                if (pAudioData->pUserData->songdataA != NULL)
                {
                        currentFilename = pAudioData->pUserData->filenameA;
                        currentSongData = pAudioData->pUserData->songdataA;
                        pAudioData->fileA = (currentFilename != NULL && strcmp(currentFilename, "") != 0) ? fopen(currentFilename, "rb") : NULL;
                }
                else
                {
                        currentFilename = NULL;
                        currentSongData = NULL;
                        pAudioData->fileA = NULL;
                }
        }
        else
        {
                if (pAudioData->fileA != NULL)
                        fclose(pAudioData->fileA);
                pAudioData->fileA = NULL;
                if (pAudioData->pUserData->songdataA != NULL)
                {
                        currentFilename = pAudioData->pUserData->filenameB;
                        currentSongData = pAudioData->pUserData->songdataB;
                        pAudioData->fileB = (currentFilename != NULL && strcmp(currentFilename, "") != 0) ? fopen(currentFilename, "rb") : NULL;
                }
                else
                {
                        currentFilename = NULL;
                        currentSongData = NULL;
                        pAudioData->fileB = NULL;
                }
        }

        pAudioData->pUserData->currentSongData = currentSongData;
        pAudioData->currentPCMFrame = 0;

        setSeekElapsed(0.0);

        setEOFReached();
}
