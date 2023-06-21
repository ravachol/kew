#include "file.h"

int deleteFile(const char *filePath)
{
    if (unlink(filePath) == 0)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

void generateTempFilePath(char *filePath, const char *prefix, const char *suffix)
{
    const char *tempDir = getenv("TMPDIR");
    if (tempDir == NULL)
    {
#ifdef __APPLE__
        tempDir = "/tmp";
#else
        tempDir = "/tmp";
#endif
    }
    char randomString[7];
    srand(time(NULL));
    for (int i = 0; i < 6; ++i)
    {
        randomString[i] = 'a' + rand() % 26;
    }
    randomString[6] = '\0';

    snprintf(filePath, FILENAME_MAX, "%s/cue%s%s%s", tempDir, prefix, randomString, suffix);
}

const char *getFileExtension(const char *filePath)
{
    const char *extension = strrchr(filePath, '.');
    if (extension != NULL)
    {
        return extension + 1; // Skip the dot character
    }
    return NULL; // No extension found
}

int openFileWithRetry(const char *filePath, const char *mode, FILE **file)
{
    int retryCount = 0;
    int result = -1;

    do {
        *file = fopen(filePath, mode);
        if (*file != NULL) {
            result = 0; // File opened successfully
            break;
        }

        retryCount++;
        usleep(RETRY_DELAY_MICROSECONDS);
    } while (retryCount < MAX_RETRY_COUNT);

    return result;
}