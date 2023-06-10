#include <string.h>
#include <dirent.h>
#include "../include/getcover/getcover.h"
#include "../include/imgtotxt/options.h"
#include "../include/imgtotxt/write_ascii.h"
#include "term.h"
#include "stringextensions.h"

int default_ascii_height = 25;
int default_ascii_width = 50;

int small_ascii_height = 18;
int small_ascii_width = 36;

int isAudioFile(const char* filename) 
{
    const char* extensions[] = {".mp3", ".wav", ".m4a", ".flac", ".ogg"};
    
    for (long unsigned int i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++) {
        if (strstr(filename, extensions[i]) != NULL) {
            return 1;
        }
    }
    
    return 0;
}

void addSlashIfNeeded(char* str) 
{
    size_t len = strlen(str);
    
    if (len > 0 && str[len-1] != '/') {
        strcat(str, "/");
    }
}

int compareEntries(const struct dirent **a, const struct dirent **b) 
{
    const char *nameA = (*a)->d_name;
    const char *nameB = (*b)->d_name;

    if (nameA[0] == '_' && nameB[0] != '_') {
        return -1;  // Directory A starts with underscore, so it should come first
    } else if (nameA[0] != '_' && nameB[0] == '_') {
        return 1;   // Directory B starts with underscore, so it should come first
    }

    return strcmp(nameA, nameB);  // Lexicographic comparison for other cases
}

char* findFirstPathWithAudioFile(const char* path) 
{
    DIR *dir = opendir(path);
    if (dir == NULL) {
        return NULL;
    }

    struct dirent **entries;
    int numEntries = scandir(path, &entries, NULL, compareEntries);

    if (numEntries < 0) {
        closedir(dir);
        return NULL;
    }

    char *audioDirectory = NULL;
    for (int i = 0; i < numEntries; i++) {
        struct dirent *entry = entries[i];
        if (S_ISDIR(entry->d_type)) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            char subDirectoryPath[MAXPATHLEN];
            snprintf(subDirectoryPath, MAXPATHLEN, "%s/%s", path, entry->d_name);

            audioDirectory = findFirstPathWithAudioFile(subDirectoryPath);
            if (audioDirectory != NULL) {
                break;
            }
        } else {
            if (isAudioFile(entry->d_name)) {
                audioDirectory = strdup(path);
                break;
            }
        }
    }

    for (int i = 0; i < numEntries; i++) {
        free(entries[i]);
    }
    free(entries);
    closedir(dir);
    return audioDirectory;
}

char* findLargestImageFile(const char* directoryPath) 
{
    DIR* directory = opendir(directoryPath);
    struct dirent* entry;
    struct stat fileStats;
    char* largestImageFile = NULL;
    off_t largestFileSize = 0;

    if (directory == NULL) {
        fprintf(stderr, "Failed to open directory: %s\n", directoryPath);
        return NULL;
    }

    while ((entry = readdir(directory)) != NULL) {
        char filePath[MAX_FILENAME_LENGTH];
        snprintf(filePath, MAX_FILENAME_LENGTH, "%s%s", directoryPath, entry->d_name);

        if (stat(filePath, &fileStats) == -1) {
            //fprintf(stderr, "Failed to get file stats: %s\n", filePath);
            continue;            
        }

        // Check if the entry is a regular file and has a larger size than the current largest image file
        if (S_ISREG(fileStats.st_mode) && fileStats.st_size > largestFileSize) {
            char* extension = strrchr(entry->d_name, '.');
            if (extension != NULL && (strcasecmp(extension, ".jpg") == 0 || strcasecmp(extension, ".jpeg") == 0 ||
                                      strcasecmp(extension, ".png") == 0 || strcasecmp(extension, ".gif") == 0)) {
                largestFileSize = fileStats.st_size;
                if (largestImageFile != NULL) {
                    free(largestImageFile);
                }
                largestImageFile = strdup(filePath);
            }
        }
    }

    if (directory != NULL) closedir(directory);
    return largestImageFile;
}


int displayAlbumArt(const char* directory, int asciiHeight, int asciiWidth)
{
    // Display Albumart ascii if available
  if (directory != NULL) {
    char* largestImageFile = findLargestImageFile(directory);

    if (largestImageFile == NULL) {
    
        char* audioDir = findFirstPathWithAudioFile(directory);

        if (audioDir != NULL)
        {
          addSlashIfNeeded(audioDir);
          largestImageFile = findLargestImageFile(audioDir);
          if (largestImageFile == NULL)
          {
            extract_cover(audioDir);
            largestImageFile = findLargestImageFile(audioDir);
          }
        }   
        free(audioDir);        
    }

    if (largestImageFile != NULL) {
        output_ascii(largestImageFile, asciiHeight, asciiWidth, ANSI);
        free(largestImageFile);
        return 0;
    }   
    else {
        return -1;
    }
  }
  return -1;
}