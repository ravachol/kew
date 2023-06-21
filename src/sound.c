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
ma_decoder *decoder = NULL;
UserData *userData = NULL;

char audioTempFilePath[FILENAME_MAX];

int convertToPcmFile(const char *filePath, const char *outputFilePath)
{
    const int COMMAND_SIZE = 1000;
    char command[COMMAND_SIZE];
    int status;

    // Construct the command string
    snprintf(command, sizeof(command),
             "ffmpeg -v fatal -hide_banner -nostdin -y -i \"%s\" -f s16le -acodec pcm_s16le -ac %d -ar %d \"%s\"",
             filePath, CHANNELS, SAMPLE_RATE, outputFilePath);
    // Execute the command
    pid_t pid = fork();

    if (pid == -1) {
        // Fork failed
        perror("fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        system(command);
        exit(EXIT_SUCCESS);
    } else {
        // Parent process
        wait(&status);  // Wait for the child process to finish
    }
    return 0;
}

void pcmCallback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
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

    (void)pInput;
}

int playPcmFile(const char *filePath)
{
    FILE *file = fopen(filePath, "rb");
    if (openFileWithRetry(filePath, "rb", &file) > 0)
        return -1;

    userData = (UserData *)malloc(sizeof(UserData));
    userData->file = file;
    ma_device_uninit(&device);
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format = ma_format_s16;
    deviceConfig.playback.channels = 2;
    deviceConfig.sampleRate = 44100;
    deviceConfig.dataCallback = pcmCallback;
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

int playAsPcm(const char *filePath)
{
    UserData *userData = (UserData *)device.pUserData;
    ma_device_stop(&device);    
    if (userData != NULL)
    {
        if (userData->decoder)
            ma_decoder_uninit((ma_decoder *)userData->decoder);
    }
    while (ma_device_get_state(&device) == ma_device_state_started) {
        usleep(100000);
    }
    ma_device_uninit(&device);
    deleteFile(audioTempFilePath);
    audioTempFilePath[0] = '\0';

    generateTempFilePath(audioTempFilePath, "temp", ".pcm");
    if (convertToPcmFile(filePath, audioTempFilePath) != 0)
    {
        return -1;
    }
    int ret = playPcmFile(audioTempFilePath);
    if (ret != 0)
    {
        return -1;
    }
    return 0;
}

int playSoundFile(const char *filePath)
{
    eofReached = 0;
    return playAsPcm(filePath);
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

void stopPlayback()
{
    ma_device_stop(&device);
}

void cleanupPlaybackDevice()
{
    UserData *userData = (UserData *)device.pUserData;
    if (userData != NULL)
    {
        if (userData->decoder)
            ma_decoder_uninit((ma_decoder *)userData->decoder);
    }
    ma_device_stop(&device);
    while (ma_device_get_state(&device) == ma_device_state_started) {
        usleep(100000);
    }
    ma_device_uninit(&device);
    deleteFile(audioTempFilePath);
    audioTempFilePath[0] = '\0';
}

int getSongDuration(const char *filePath, double *duration)
{
    char command[1024];
    FILE *pipe;
    char output[1024];
    char *durationStr;
    double durationValue;

    snprintf(command, sizeof(command), "ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 \"%s\"", filePath);

    pipe = popen(command, "r");
    if (pipe == NULL)
    {
        fprintf(stderr, "Failed to run FFprobe command.\n");
        return -1;
    }
    if (fgets(output, sizeof(output), pipe) == NULL)
    {
        fprintf(stderr, "Failed to read FFprobe output.\n");
        pclose(pipe);
        return -1;
    }
    pclose(pipe);

    durationStr = strtok(output, "\n");
    if (durationStr == NULL)
    {
        fprintf(stderr, "Failed to parse FFprobe output.\n");
        return -1;
    }
    durationValue = atof(durationStr);
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
    FILE *fp = popen(command_str, "r");
    if (fp == NULL)
    {
        return -1;
    }
    pclose(fp);
    return 0;
}