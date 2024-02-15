#define _XOPEN_SOURCE 700
#define __USE_XOPEN_EXTENDED 1
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <dirent.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixfmt.h>
#include "file.h"
#include "../include/imgtotxt/options.h"
#include "metadata.h"
#include "stringfunc.h"
#include "term.h"
#include "albumart.h"
#include "cache.h"
#include "chafafunc.h"

/*

albumart.c

 Functions related to extracting album art from audio files or directories.
 
*/
#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

FIBITMAP *bitmap;
int indent = 0;

int extractCover(const char *inputFilePath, const char *outputFilePath) {
    AVFormatContext *fmt_ctx = NULL;
    int ret;
    AVPacket pkt;

    if ((ret = avformat_open_input(&fmt_ctx, inputFilePath, NULL, NULL)) < 0) {
        fprintf(stderr, "Could not open input file '%s'\n", inputFilePath);
        return -1;
    }

    int stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (stream_index < 0) {
        fprintf(stderr, "Could not find a video stream in the input file\n");
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    AVStream *video_stream = fmt_ctx->streams[stream_index];
    if (video_stream->disposition & AV_DISPOSITION_ATTACHED_PIC) {

        AVPacket *pkt = &video_stream->attached_pic;

        FILE *file = fopen(outputFilePath, "wb");
        if (!file) {
            fprintf(stderr, "Could not open output file '%s'\n", outputFilePath);
            avformat_close_input(&fmt_ctx);
            return -1;
        }
        fwrite(pkt->data, 1, pkt->size, file);
        fclose(file);

        avformat_close_input(&fmt_ctx);
        return 1;
    } else {
        avformat_close_input(&fmt_ctx);
        return -1;
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
                snprintf(filePath, sizeof(filePath), "%s/%s", directoryPath, entry->d_name);

                if (stat(filePath, &fileStats) == -1)
                {
                        // fprintf(stderr, "Failed to get file stats: %s\n", filePath);
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

int calcIdealImgSize(int *width, int *height, const int visualizerHeight, const int metatagHeight)
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        int timeDisplayHeight = 1;
        int heightMargin = 2;
        int minHeight = visualizerHeight + metatagHeight + timeDisplayHeight + heightMargin;
        *height = term_h - minHeight;
        *width = ceil(*height * 2);
        if (*width > term_w)
        {
                *width = term_w;
                *height = floor(*width / 2);
        }
        int remainder = *width % 2;
        if (remainder == 1)
        {
                *width -= 1;
        }
        return 0;
}

int getCoverIndent(int desired_width)
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        int indent = ((term_w - desired_width) / 2) + 1;
        return (indent > 0) ? indent : 0;
}

int displayCover(SongData *songdata, int width, int height, bool ansii)
{
        if (!ansii)
        {
                clearScreen();
                printBitmapCentered(songdata->cover, width - 1, height - 2);
        }
        else
        {
                cursorJump(1);
                PixelData pixel = {*songdata->red, *songdata->green, *songdata->blue};
                output_ascii(songdata->coverArtPath, height - 2, width - 1, &pixel);
        }
        printf("\n");

        return 0;
}
