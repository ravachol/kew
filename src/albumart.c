#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <dirent.h>
#include "file.h"
#include "../include/getcover/getcover.h"
#include "../include/imgtotxt/options.h"
#include "metadata.h"
#include "stringfunc.h"
#include "term.h"
#include "albumart.h"
#include "cache.h"

char coverArtFilePath[MAXPATHLEN];

void runChafaCommand(const char *filepath, int width, int height)
{
    const int COMMAND_SIZE = 1000;
    char command[COMMAND_SIZE];
    int status;

    snprintf(command, COMMAND_SIZE, "chafa --clear -s %dx%d --stretch -C on \"%s\"", width, height, filepath);

    pid_t pid = fork();

    if (pid == -1) {
        // Fork failed
        perror("fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        execl("/bin/sh", "sh", "-c", command, (char *)NULL);
        exit(EXIT_SUCCESS);
    } else {
        // Parent process
        waitpid(pid, &status, 0);  // Wait for the chafa process to finish
    }
}


int isAudioFile(const char *filename)
{
    const char *extensions[] = {".mp3", ".wav", ".m4a", ".flac", ".ogg"};

    for (long unsigned int i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++)
    {
        if (strstr(filename, extensions[i]) != NULL)
        {
            return 1;
        }
    }
    return 0;
}

void addSlashIfNeeded(char *str)
{
    size_t len = strlen(str);

    if (len > 0 && str[len - 1] != '/')
    {
        strcat(str, "/");
    }
}

int compareEntries(const struct dirent **a, const struct dirent **b)
{
    const char *nameA = (*a)->d_name;
    const char *nameB = (*b)->d_name;

    if (nameA[0] == '_' && nameB[0] != '_')
    {
        return -1; // Directory A starts with underscore, so it should come first
    }
    else if (nameA[0] != '_' && nameB[0] == '_')
    {
        return 1; // Directory B starts with underscore, so it should come first
    }

    return strcmp(nameA, nameB); // Lexicographic comparison for other cases
}

char *findFirstPathWithAudioFile(const char *path)
{
    DIR *dir = opendir(path);
    if (dir == NULL)
    {
        return NULL;
    }

    struct dirent **entries;
    int numEntries = scandir(path, &entries, NULL, compareEntries);

    if (numEntries < 0)
    {
        closedir(dir);
        return NULL;
    }

    char *audioDirectory = NULL;
    for (int i = 0; i < numEntries; i++)
    {
        struct dirent *entry = entries[i];
        if (S_ISDIR(entry->d_type))
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            {
                continue;
            }
            char subDirectoryPath[MAXPATHLEN];
            snprintf(subDirectoryPath, MAXPATHLEN, "%s/%s", path, entry->d_name);

            audioDirectory = findFirstPathWithAudioFile(subDirectoryPath);
            if (audioDirectory != NULL)
            {
                break;
            }
        }
        else
        {
            if (isAudioFile(entry->d_name))
            {
                audioDirectory = strdup(path);
                break;
            }
        }
    }

    for (int i = 0; i < numEntries; i++)
    {
        free(entries[i]);
    }
    free(entries);
    closedir(dir);
    return audioDirectory;
}

char *findLargestImageFileRecursive(const char *directoryPath, char *largestImageFile, off_t *largestFileSize)
{
    DIR *directory = opendir(directoryPath);
    struct dirent *entry;
    struct stat fileStats;

    if (directory == NULL)
    {
        fprintf(stderr, "Failed to open directory: %s\n", directoryPath);
        return largestImageFile;
    }

    while ((entry = readdir(directory)) != NULL)
    {
        char filePath[MAX_FILENAME_LENGTH];
        snprintf(filePath, MAX_FILENAME_LENGTH, "%s/%s", directoryPath, entry->d_name);

        if (stat(filePath, &fileStats) == -1)
        {
            // fprintf(stderr, "Failed to get file stats: %s\n", filePath);
            continue;
        }

        if (S_ISDIR(fileStats.st_mode))
        {
            // Skip "." and ".." directories
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            {
                continue;
            }

            // Recursive call to process subdirectories
            largestImageFile = findLargestImageFileRecursive(filePath, largestImageFile, largestFileSize);
        }
        else if (S_ISREG(fileStats.st_mode))
        {
            // Check if the entry is an image file and has a larger size than the current largest image file
            char *extension = strrchr(entry->d_name, '.');
            if (extension != NULL && (strcasecmp(extension, ".jpg") == 0 || strcasecmp(extension, ".jpeg") == 0 ||
                                      strcasecmp(extension, ".png") == 0 || strcasecmp(extension, ".gif") == 0))
            {
                if (fileStats.st_size > *largestFileSize)
                {
                    *largestFileSize = fileStats.st_size;
                    if (largestImageFile != NULL)
                    {
                        free(largestImageFile);
                    }
                    largestImageFile = strdup(filePath);
                }
            }
        }
    }

    closedir(directory);
    return largestImageFile;
}

char *findLargestImageFile(const char *directoryPath)
{
    DIR *directory = opendir(directoryPath);
    struct dirent *entry;
    struct stat fileStats;
    char *largestImageFile = NULL;
    off_t largestFileSize = 0;

    if (directory == NULL)
    {
        fprintf(stderr, "Failed to open directory: %s\n", directoryPath);
        return NULL;
    }

    while ((entry = readdir(directory)) != NULL)
    {
        char filePath[MAX_FILENAME_LENGTH];
        snprintf(filePath, MAX_FILENAME_LENGTH, "%s%s", directoryPath, entry->d_name);

        if (stat(filePath, &fileStats) == -1)
        {
            // fprintf(stderr, "Failed to get file stats: %s\n", filePath);
            continue;
        }

        // Check if the entry is a regular file and has a larger size than the current largest image file
        if (S_ISREG(fileStats.st_mode) && fileStats.st_size > largestFileSize)
        {
            char *extension = strrchr(entry->d_name, '.');
            if (extension != NULL && (strcasecmp(extension, ".jpg") == 0 || strcasecmp(extension, ".jpeg") == 0 ||
                                      strcasecmp(extension, ".png") == 0 || strcasecmp(extension, ".gif") == 0))
            {
                largestFileSize = fileStats.st_size;
                if (largestImageFile != NULL)
                {
                    free(largestImageFile);
                }
                largestImageFile = strdup(filePath);
            }
        }
    }

    if (directory != NULL)
        closedir(directory);
    return largestImageFile;
}

int extractCover(const char *audioFilepath, char *outputFilepath)
{
    size_t size;
    char str[10] = "";
    FILE *fp;
    char path[MAXPATHLEN];

    if (audioFilepath == NULL)
        return -1;

    getDirectoryFromPath(audioFilepath, path);
    generateTempFilePath(outputFilepath, "cover", ".jpg");

    if ((fp = fopen(audioFilepath, "r")) != NULL)
    {
        size = fread(str, 1, 4, fp);
        if (memcmp(str, "fLaC", 4) == 0)
        {
            if (get_FLAC_cover(fp, outputFilepath) == 1)
            {
                fclose(fp);
                return 1;
            }
        }
        else if (memcmp(str, "RIFF", 4) == 0) // Wav file
        {
        }
        else if (memcmp(str, "ID3\3", 4) == 0) // Mp3
        {
            size_t pathLength = strlen(path);
            size_t coverLength = strlen("cover.jpg");

            if (extractMP3Cover(audioFilepath, outputFilepath) >= 0)
            {
                fclose(fp);
                return 1;
            }
        }
        else
        {
            int offset = ((str[0] << 24) | (str[1] << 16) | (str[2] << 8) | str[3]);
            size = fread(str, 1, 4, fp);
            if (memcmp(str, "ftyp", 4) == 0) // MP4
            {
                if (get_m4a_cover(fp, outputFilepath) == 1)
                {
                    fclose(fp);
                    return 1;
                }
            }
        }
        fclose(fp);
    }
    return -1;
}

int calcIdealImgSize(int *width, int *height, const int equalizerHeight, const int metatagHeight, bool firstSong)
{
    int term_w, term_h;
    getTermSize(&term_w, &term_h);
    int timeDisplayHeight = 1;
    int heightMargin = 2;
    int widthMargin = 2;
    int minHeight = equalizerHeight + metatagHeight + timeDisplayHeight + heightMargin;
    *height = term_h - minHeight;
    *width = ceil(*height * 2);
    if (*width > term_w)
    {
        *width = term_w;
        *height = floor(*width / 2);
    }
    int remainder = *width % 2;
    if (remainder == 1)
        *width -= 1;
    *width - widthMargin; // compensate for first character on each line being a blank
    return 0;
}

int displayAlbumArt(const char *filepath, int width, int height, bool coverBlocks, PixelData *brightPixel)
{
    char path[MAXPATHLEN];

    if (filepath == NULL)
        return -1;

    if (coverArtFilePath[0] == '\0')
    {
        int res = extractCover(filepath, coverArtFilePath);
        if (res < 0)
        {
            getDirectoryFromPath(filepath, path);  
            coverArtFilePath[0] == '\0';

            char *tmp = NULL;
            off_t size;
            tmp = findLargestImageFileRecursive(path, tmp, &size);

            if (tmp != NULL)
                strcpy(coverArtFilePath, tmp);
            else
                return -1;
        }
        else {
            addToCache(tempCache, coverArtFilePath);
        }
        if (coverArtFilePath == '\0')
            return -1;
    }
    if (coverBlocks)
    {
        getBrightPixel(coverArtFilePath, width, height, brightPixel);      
        // As little margins as possible when running chafa, since it cannot create a left margin
        runChafaCommand(coverArtFilePath, width, height);
    }
    else
    {
        cursorJump(1);
        width--;
        output_ascii(coverArtFilePath, height, width, coverBlocks, brightPixel);
    }
    printf(" \n");        
    return 0;
}