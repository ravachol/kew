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

    snprintf(filePath, FILENAME_MAX, "%s/%sXXXXXX%s", tempDir, prefix, suffix);
    mkstemp(filePath);
}

const char *getFileExtension(const char *filePath) {
    const char *extension = strrchr(filePath, '.');
    if (extension != NULL) {
        return extension + 1; // Skip the dot character
    }
    return NULL; // No extension found
}
