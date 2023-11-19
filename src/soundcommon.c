#include "soundcommon.h"

#define MAX_DECODERS 2

const char BUILTIN_EXTENSIONS[] = "\\.(mp3|flac|wav)$";

bool repeatEnabled = false;
bool shuffleEnabled = false;
bool skipToNext = false;
bool seekRequested = false;
bool paused = false;
float seekPercent = 0.0;
double seekElapsed;
_Atomic bool EOFReached = false;
ma_device device = {0};
ma_int32 *audioBuffer = NULL;
ma_decoder *firstDecoder;
ma_decoder *currentDecoder;
int bufSize;
ma_event switchAudioImpl;
enum AudioImplementation currentImplementation = NONE;
ma_decoder *decoders[MAX_DECODERS];

int decoderIndex = -1;

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

        if (firstDecoder != NULL)
        {
                ma_decoder_uninit(firstDecoder);
                free(firstDecoder);
                firstDecoder = NULL;
        }
        if (decoders[0] != NULL)
        {
                ma_decoder_uninit(decoders[0]);
                free(decoders[0]);
                decoders[0] = NULL;
        }
        if (decoders[1] != NULL)
        {
                ma_decoder_uninit(decoders[1]);
                free(decoders[1]);
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

void prepareNextDecoder(char *filepath)
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

        uninitPreviousDecoder();

        ma_decoder_config config = ma_decoder_config_init(ma_format_s24, 2, 192000);
        ma_decoder *decoder = (ma_decoder *)malloc(sizeof(ma_decoder));
        ma_decoder_init_file(filepath, NULL, decoder);
        setNextDecoder(decoder);
        if (currentDecoder != NULL)
                ma_data_source_set_next(currentDecoder, decoder);
}

void getFileInfo(const char *filename, ma_uint32 *sampleRate, ma_uint32 *channels, ma_format *format)
{
        ma_decoder decoder;
        if (ma_decoder_init_file(filename, NULL, &decoder) == MA_SUCCESS)
        {
                *sampleRate = decoder.outputSampleRate;
                *channels = decoder.outputChannels;
                *format = decoder.outputFormat;
                ma_decoder_uninit(&decoder);
        }
        else
        {
                // Handle file open error.
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

ma_int32 *getAudioBuffer()
{
        return audioBuffer;
}

void setAudioBuffer(ma_int32 *buf)
{
        audioBuffer = buf;
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

void setSeekElapsed(double value)
{
        seekElapsed = value;
}

bool isEOFReached()
{
        return atomic_load(&EOFReached) ? true : false;
}

void setEOFReached()
{
        atomic_store(&EOFReached, true);
}

void setEOFNotReached()
{
        atomic_store(&EOFReached, false);
}

void skip()
{
        currentImplementation = NONE;
        setSkipToNext(true);
        setRepeatEnabled(false);
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
}

void pausePlayback()
{
        if (ma_device_is_started(&device))
        {
                ma_device_stop(&device);
                paused = true;
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
                ma_device_stop(&device);
                paused = true;
        }
        else if (paused)
        {
                resumePlayback();
                paused = false;
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
        char exto[6];
        extractExtension(filePath, sizeof(exto) - 1, exto);
        regex_t regex;
        int ret = regcomp(&regex, BUILTIN_EXTENSIONS, REG_EXTENDED);
        if (ret != 0)
        {
                printf("Failed to compile regular expression\n");
                return false;
        }
        return (match_regex(&regex, exto) == 0);
}

void activateSwitch(PCMFileDataSource *pPCMDataSource)
{
        setSkipToNext(false);
        if (!isRepeatEnabled())
                pPCMDataSource->currentFileIndex = 1 - pPCMDataSource->currentFileIndex; // Toggle between 0 and 1
        pPCMDataSource->switchFiles = true;
}

void executeSwitch(PCMFileDataSource *pPCMDataSource)
{
        pPCMDataSource->switchFiles = false;
        switchDecoder();
        
        ma_uint64 length = 0;
        ma_data_source_get_length_in_pcm_frames(getCurrentDecoder(), &length);
        pPCMDataSource->totalFrames = length;

        // Close the current file, and open the new one
        FILE *previousFile;
        char *currentFilename;
        SongData *currentSongData;

        if (pPCMDataSource->currentFileIndex == 0)
        {
                previousFile = pPCMDataSource->fileB;
                currentFilename = pPCMDataSource->pUserData->filenameA;
                currentSongData = pPCMDataSource->pUserData->songdataA;
                pPCMDataSource->fileA = (currentFilename != NULL && strcmp(currentFilename, "") != 0) ? fopen(currentFilename, "rb") : NULL;
        }
        else
        {
                previousFile = pPCMDataSource->fileA;
                currentFilename = pPCMDataSource->pUserData->filenameB;
                currentSongData = pPCMDataSource->pUserData->songdataB;            
                pPCMDataSource->fileB = (currentFilename != NULL && strcmp(currentFilename, "") != 0) ? fopen(currentFilename, "rb") : NULL;
        }

        pPCMDataSource->pUserData->currentSongData = currentSongData;
        pPCMDataSource->currentPCMFrame = 0;

        if (previousFile != NULL)
                fclose(previousFile);

        setSeekElapsed(0.0);

        setEOFReached();
}
