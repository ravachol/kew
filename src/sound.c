#define MINIAUDIO_IMPLEMENTATION
#include "sound.h"

#define CHANNELS 2
#define SAMPLE_RATE 44100
#define SAMPLE_WIDTH 2
#define FRAMES_PER_BUFFER 1024

typedef struct
{
    FILE *file;
    ma_decoder *decoder;
} UserData;

ma_int16 *g_audioBuffer = NULL;
static int eofReached = 0;
ma_device device = {0};
UserData *userData = NULL;

void data_callback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
{
    if (userData == NULL)
    {
        return;
    }
    ma_decoder *pDecoder = (ma_decoder *)userData->decoder;
    if (pDecoder == NULL)
    {
        return;
    }

    ma_uint64 framesRead;
    ma_decoder_read_pcm_frames(pDecoder, pOutput, frameCount, &framesRead);

    if (framesRead < frameCount)
    {
        eofReached = 1;
    }

    ma_format outputFormat = pDevice->playback.format;
    size_t bytesPerFrame = ma_get_bytes_per_frame(outputFormat, pDevice->playback.channels);
    size_t bufferSizeInBytes = frameCount * bytesPerFrame;

    g_audioBuffer = (ma_int16 *)malloc(bufferSizeInBytes);
    if (g_audioBuffer == NULL)
    {
        return;
    }

    // Conversion from output format to ma_int16
    switch (outputFormat)
    {
    case ma_format_f32:
    {
        const float *outputF32 = (const float *)pOutput;
        for (size_t i = 0; i < frameCount; ++i)
        {
            float sampleFloat = outputF32[i];
            ma_int16 sampleInt16 = (ma_int16)(sampleFloat * 32767.0f);
            g_audioBuffer[i] = sampleInt16;
        }
        break;
    }
    case ma_format_s24:
    {
        const ma_uint8 *outputS24 = (const ma_uint8 *)pOutput;
        for (size_t i = 0; i < frameCount; ++i)
        {
            ma_int32 sampleInt24 = (outputS24[i * 3] << 8) | (outputS24[i * 3 + 1] << 16) | (outputS24[i * 3 + 2] << 24);
            sampleInt24 >>= 8; // Shift right to get 24-bit sample to 16 bits
            ma_int16 sampleInt16 = (ma_int16)sampleInt24;
            g_audioBuffer[i] = sampleInt16;
        }
        break;
    }
    case ma_format_s16:
        memcpy(g_audioBuffer, pOutput, bufferSizeInBytes);
        break;
    default:
        break;
    }

    (void)pInput;
}

int playSoundFileDefault(const char *filePath)
{
    if (filePath == NULL)
        return -1;

    ma_result result;
    ma_decoder *decoder = (ma_decoder *)malloc(sizeof(ma_decoder));
    if (decoder == NULL)
    {
        return -1; // Failed to allocate memory for decoder
    }

    result = ma_decoder_init_file(filePath, NULL, decoder);
    if (result != MA_SUCCESS)
    {
        free(decoder);
        return -2;
    }

    if (userData == NULL)
    {
        userData = (UserData *)malloc(sizeof(UserData));
        if (userData == NULL)
        {
            ma_decoder_uninit(decoder);
            free(decoder);
            return -3; // Failed to allocate memory for user data
        }
    }

    userData->decoder = decoder;

    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format = decoder->outputFormat;
    deviceConfig.playback.channels = decoder->outputChannels;
    deviceConfig.sampleRate = decoder->outputSampleRate;
    deviceConfig.dataCallback = data_callback;
    deviceConfig.pUserData = userData;

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS)
    {
        printf("Failed to open playback device.\n");
        ma_decoder_uninit(decoder);
        free(decoder);
        return -4;
    }

    if (ma_device_start(&device) != MA_SUCCESS)
    {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&device);
        ma_decoder_uninit(decoder);
        free(decoder);
        return -5;
    }

    return 0;
}

int convertAacToPcmFile(const char *filePath, const char *outputFilePath)
{
    char ffmpegCommand[1024];
    snprintf(ffmpegCommand, sizeof(ffmpegCommand),
             "ffmpeg -v fatal -hide_banner -nostdin -y -i \"%s\" -f s16le -acodec pcm_s16le -ac %d -ar %d \"%s\"",
             filePath, CHANNELS, SAMPLE_RATE, outputFilePath);

    int res = system(ffmpegCommand); // Execute FFmpeg command to create the output.pcm file

    if (res != 0)
    {
        return -1;
    }

    return 0;
}

void pcm_callback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
{
    UserData *pUserData = (UserData *)pDevice->pUserData;
    if (pUserData == NULL)
    {
        return;
    }

    size_t framesRead = fread(pOutput, ma_get_bytes_per_frame(pDevice->playback.format, pDevice->playback.channels), frameCount, pUserData->file);
    if (framesRead < frameCount)
    {
        eofReached = 1;
    }

    // Allocate memory for g_audioBuffer (if not already allocated)
    if (g_audioBuffer == NULL)
    {
        g_audioBuffer = malloc(sizeof(ma_int16) * frameCount);
        if (g_audioBuffer == NULL)
        {
            // Memory allocation failed
            return;
        }
    }

    // Copy the audio samples from pOutput to audioBuffer
    memcpy(g_audioBuffer, pOutput, sizeof(ma_int16) * frameCount);
    // memcpy(g_audioBuffer, pOutput, bufferSize);

    (void)pInput;
}

int playPcmFile(const char *filePath)
{
    FILE *file = fopen(filePath, "rb");
    if (file == NULL)
    {
        printf("Could not open file: %s\n", filePath);
        return -2;
    }

    userData = (UserData *)malloc(sizeof(UserData));
    userData->file = file;
    ma_device_uninit(&device);
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format = ma_format_s16;
    deviceConfig.playback.channels = 2;
    deviceConfig.sampleRate = 44100;
    deviceConfig.dataCallback = pcm_callback;
    deviceConfig.pUserData = userData;

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS)
    {
        printf("Failed to open playback device.\n");
        fclose(file);
        return -3;
    }

    if (ma_device_start(&device) != MA_SUCCESS)
    {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&device);
        fclose(file);
        return -4;
    }

    return 0;
}

char tempFilePath[FILENAME_MAX];

int playAacFile(const char *filePath)
{
    generateTempFilePath(tempFilePath, "temp", ".pcm");
    if (convertAacToPcmFile(filePath, tempFilePath) != 0)
    {
        // printf("Failed to run FFmpeg command:\n");
        return -1;
    }

    int ret = playPcmFile(tempFilePath);

    if (ret != 0)
    {
        // printf("Failed to play file: %d\n", ret);
        return -1;
    }

    // delete pcm file

    return 0;
}

int playSoundFile(const char *filePath)
{
    eofReached = 0;
    int ret = playSoundFileDefault(filePath);
    if (ret != 0)
        ret = playAacFile(filePath);
    return ret;
}

int playPlaylist(char *filePath)
{
    return 0;
}

bool paused = false;

void resumePlayback()
{
    if (!ma_device_is_started(&device))
    {
        ma_device_start(&device);
    }
}

void pausePlayback()
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

int isPlaybackDone()
{
    if (eofReached == 1)
    {
        eofReached = 0;
        return 1;
    }
    else
    {
        return 0;
    }
}

void cleanupPlaybackDevice()
{
    UserData *userData = (UserData *)device.pUserData;
    if (userData != NULL)
    {
        if (userData->decoder)
            ma_decoder_uninit((ma_decoder *)userData->decoder);
    }
    ma_device_uninit(&device);
    deleteFile(tempFilePath);
}

int getSongDuration(const char *filePath, double *duration)
{
    char command[1024];
    FILE *pipe;
    char output[1024];
    char *durationStr;
    double durationValue;

    // Construct the FFprobe command
    snprintf(command, sizeof(command), "ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 \"%s\"", filePath);

    // Open a pipe to execute the command
    pipe = popen(command, "r");
    if (pipe == NULL)
    {
        fprintf(stderr, "Failed to run FFprobe command.\n");
        return -1;
    }

    // Read the output of the command
    if (fgets(output, sizeof(output), pipe) == NULL)
    {
        fprintf(stderr, "Failed to read FFprobe output.\n");
        pclose(pipe);
        return -1;
    }

    // Close the pipe
    pclose(pipe);

    // Extract the duration value from the output
    durationStr = strtok(output, "\n");
    if (durationStr == NULL)
    {
        fprintf(stderr, "Failed to parse FFprobe output.\n");
        return -1;
    }

    // Convert the duration string to a double value
    durationValue = atof(durationStr);

    // Assign the duration value to the output parameter
    *duration = durationValue;

    return 0;
}

double getDuration(const char *filepath)
{
    double duration = 0.0;
    getSongDuration(filepath, &duration);
    return duration;
}

int adjustVolumePercent(int volumeChange)
{
    char command_str[1000];

    if (volumeChange > 0)
        snprintf(command_str, 1000, "amixer -D pulse sset Master %d%%+", volumeChange);
    else
        snprintf(command_str, 1000, "amixer -D pulse sset Master %d%%-", -volumeChange);

    // Open the command for reading.
    FILE *fp = popen(command_str, "r");
    if (fp == NULL)
    {
        return -1;
    }

    // Close the command stream.
    pclose(fp);
    return 0;
}