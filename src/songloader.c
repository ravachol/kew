#include <glib.h>
#include <gio/gio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include "tagLibWrapper.h"
#include "cache.h"
#include "imgfunc.h"
#include "file.h"
#include "sound.h"
#include "soundcommon.h"
#include "utils.h"
#include "songloader.h"
#include "stb_image.h"
/*

songloader.c

 This file should contain only functions related to loading song data.

*/

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

static guint track_counter = 0;

void makeFilePath(char *dirPath, char *filePath, struct dirent *entry){
    if(dirPath[strnlen(dirPath, MAXPATHLEN)-1] == '/'){
        sprintf(filePath, "%s%s", dirPath, entry->d_name);
    }else{
        sprintf(filePath, "%s/%s", dirPath, entry->d_name);
    }
}

char *chooseAlbumArt(char *dirPath, char **customFileNameArr, int size){

        DIR *directory = opendir(dirPath);
        struct dirent *entry;
        struct stat fileStat;
    
        // Check if selected directory is empty //
        if(directory != NULL){
    
            // If it's not empty go through all the files in it and file paths and match files / extension with prio list //
            char *result = NULL;
            for(char **ptr = customFileNameArr; ptr<customFileNameArr+size; ptr++){
                
                rewinddir(directory);
    
                while ((entry = readdir(directory)) != NULL){
    
                    // Handle hidden folders etc //
                    if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
                        continue;
                    }
    
                    // Create required data //
                    char filePath[MAXPATHLEN];
                    makeFilePath(dirPath, filePath, entry);
                    char *fileExt = strrchr(entry->d_name, '.'); // extension of the current file that's in loop //
                    char *sampleExt = strrchr(*ptr, '.'); // extension of the current custom file extension that's in loop //
                    char starExt[50];
                    sprintf(starExt, "*%s", sampleExt); // add a star before the extension to match with names like *.jpg, *.png etc //
                    logger_str(filePath, "File Path:");
                    // Using those file path check if the paths lead ot files or directories //
                    if(stat(filePath, &fileStat) == 0){
    
                        if(S_ISDIR(fileStat.st_mode)){
                            result = chooseAlbumArt(filePath, customFileNameArr, size);
                            if(result != NULL){
                                break; // if album art is found in the directory, break, else keep searching //
                            }
                        }else{
                            if(strcmp(entry->d_name, *ptr) == 0){
                                result = filePath;
                                break;
                            }else if(sampleExt && strcmp(starExt, *ptr) == 0){
                                if(fileExt && sampleExt && strcmp(fileExt, sampleExt) == 0){
                                    result = filePath;
                                    break;
                                }                           
                            }
                        }
                    }
                }
                if(result){
                    break;
                }
            }
            if(result){
                closedir(directory);
                return strdup(result);
            }
    
        }
    
        closedir(directory);
        return NULL;
}

char *findLargestImageFile(const char *directoryPath, char *largestImageFile, off_t *largestFileSize)
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
                char filePath[MAXPATHLEN];

                if (directoryPath[strnlen(directoryPath, MAXPATHLEN) - 1] == '/')
                {
                        snprintf(filePath, sizeof(filePath), "%s%s", directoryPath, entry->d_name);
                }
                else
                {
                        snprintf(filePath, sizeof(filePath), "%s/%s", directoryPath, entry->d_name);
                }

                if (stat(filePath, &fileStats) == -1)
                {
                        continue;
                }

                if (S_ISREG(fileStats.st_mode))
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

// Generate a new track ID
gchar *generateTrackId(void)
{
        gchar *trackId = g_strdup_printf("/org/kew/tracklist/track%d", track_counter);
        track_counter++;
        return trackId;
}

void loadColor(SongData *songdata)
{
        getCoverColor(songdata->cover, songdata->coverWidth, songdata->coverHeight, &(songdata->red), &(songdata->green), &(songdata->blue));
}

void loadMetaData(SongData *songdata, AppState *state)
{
        char path[MAXPATHLEN];

        songdata->metadata = malloc(sizeof(TagSettings));
        songdata->metadata->replaygainTrack = 0.0;
        songdata->metadata->replaygainAlbum = 0.0;

        generateTempFilePath(songdata->coverArtPath, "cover", ".jpg");

        int res = extractTags(songdata->filePath, songdata->metadata, &(songdata->duration), songdata->coverArtPath);

        if (res == -2)
        {
                songdata->hasErrors = true;
                return;
        }
        else if (res == -1)
        {
                getDirectoryFromPath(songdata->filePath, path);
                char *tmp = NULL;
                off_t size = 0;
                char *fileArr[21] = {"front.png","front.jpg","front.jpeg","folder.png","folder.jpg","folder.jpeg","cover.png","cover.jpg","cover.jpeg","f.png","f.jpg","f.jpeg","*front*.png","*front*.jpg",
                        "*front*.jpeg","*cover*.png","*cover*.jpg","*cover*.jpeg","*folder*.png","*folder*.jpg","*fol1der*.jpeg"};
                tmp = chooseAlbumArt(path, fileArr, 21);
                if(tmp == NULL){
                        tmp = findLargestImageFile(path, tmp, &size);
                }     

                if (tmp != NULL)
                {
                        c_strcpy(songdata->coverArtPath, tmp, sizeof(songdata->coverArtPath));
                        free(tmp);
                        tmp = NULL;
                }
                else
                        c_strcpy(songdata->coverArtPath, "", sizeof(songdata->coverArtPath));
        }
        else
        {
                addToCache(state->tmpCache, songdata->coverArtPath);
        }

        songdata->cover = getBitmap(songdata->coverArtPath, &(songdata->coverWidth), &(songdata->coverHeight));
}

SongData *loadSongData(char *filePath, AppState *state)
{
        SongData *songdata = NULL;
        songdata = malloc(sizeof(SongData));
        songdata->trackId = generateTrackId();
        songdata->hasErrors = false;
        c_strcpy(songdata->filePath, "", sizeof(songdata->filePath));
        c_strcpy(songdata->coverArtPath, "", sizeof(songdata->coverArtPath));
        songdata->red = defaultColor;
        songdata->green = defaultColor;
        songdata->blue = defaultColor;
        songdata->metadata = NULL;
        songdata->cover = NULL;
        songdata->duration = 0.0;
        songdata->avgBitRate = 0;
        c_strcpy(songdata->filePath, filePath, sizeof(songdata->filePath));
        loadMetaData(songdata, state);
        loadColor(songdata);
        return songdata;
}

void unloadSongData(SongData **songdata, AppState *state)
{
        if (*songdata == NULL)
                return;

        SongData *data = *songdata;

        if (data->cover != NULL)
        {
                stbi_image_free(data->cover);
                data->cover = NULL;
        }

        if (existsInCache(state->tmpCache, data->coverArtPath) && isInTempDir(data->coverArtPath))
        {
                deleteFile(data->coverArtPath);
        }

        free(data->metadata);
        free(data->trackId);

        data->cover = NULL;
        data->metadata = NULL;

        data->trackId = NULL;

        free(*songdata);
        *songdata = NULL;
}
