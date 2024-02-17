#include "songloader.h"
/*

songloader.c

 This file should contain only functions related to loading song data.

*/

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

void removeTagPrefix(char *value)
{
        char *colon_pos = strchr(value, ':');
        if (colon_pos)
        {
                // Remove the tag prefix by shifting the characters
                memmove(value, colon_pos + 1, strlen(colon_pos));
        }
}

void turnFilePathIntoTitle(const char *filePath, char *title)
{
        char *lastSlash = strrchr(filePath, '/');
        char *lastDot = strrchr(filePath, '.');
        if (lastSlash != NULL && lastDot != NULL && lastDot > lastSlash)
        {
                size_t maxSize = sizeof(title) - 1; // Reserve space for null terminator
                snprintf(title, maxSize, "%s", lastSlash + 1);
                memcpy(title, lastSlash + 1, lastDot - lastSlash - 1); // Copy up to dst_size - 1 bytes
                title[lastDot - lastSlash - 1] = '\0';
                trim(title);
        }
}

// Extracts metadata, returns -1 if no album cover found
int extractTags(const char *input_file, TagSettings *tag_settings, double *duration, const char *coverFilePath)
{
        AVFormatContext *fmt_ctx = NULL;
        AVDictionaryEntry *tag = NULL;
        int ret;

        if ((ret = avformat_open_input(&fmt_ctx, input_file, NULL, NULL)) < 0)
        {
                fprintf(stderr, "Could not open input file '%s'\n", input_file);
                return 1;
        }
        if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0)
        {
                fprintf(stderr, "Could not find stream information\n");
                avformat_close_input(&fmt_ctx);
                return 1;
        }

        memset(tag_settings->title, 0, sizeof(tag_settings->title));
        memset(tag_settings->artist, 0, sizeof(tag_settings->artist));
        memset(tag_settings->album_artist, 0, sizeof(tag_settings->album_artist));
        memset(tag_settings->album, 0, sizeof(tag_settings->album));
        memset(tag_settings->date, 0, sizeof(tag_settings->date));

        while ((tag = av_dict_get(fmt_ctx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
        {
                if (strcasecmp(tag->key, "title") == 0)
                {
                        snprintf(tag_settings->title, sizeof(tag_settings->title), "%s", tag->value);
                }
                else if (strcasecmp(tag->key, "artist") == 0)
                {
                        snprintf(tag_settings->artist, sizeof(tag_settings->artist), "%s", tag->value);
                }
                else if (strcasecmp(tag->key, "album_artist") == 0)
                {
                        snprintf(tag_settings->album_artist, sizeof(tag_settings->album_artist), "%s", tag->value);
                }
                else if (strcasecmp(tag->key, "album") == 0)
                {
                        snprintf(tag_settings->album, sizeof(tag_settings->album), "%s", tag->value);
                }
                else if (strcasecmp(tag->key, "date") == 0)
                {
                        snprintf(tag_settings->date, sizeof(tag_settings->date), "%s", tag->value);
                }
        }

        if (strlen(tag_settings->title) <= 0)
        {
                char title[MAXPATHLEN];
                turnFilePathIntoTitle(input_file, title);
                strncpy(tag_settings->title, title, sizeof(tag_settings->title) - 1);
                tag_settings->title[sizeof(tag_settings->title) - 1] = '\0';
        }

        if (fmt_ctx->duration != AV_NOPTS_VALUE)
        {
                *duration = (double)(fmt_ctx->duration / AV_TIME_BASE);
        }
        else
        {
                *duration = 0.0;
        }

        int stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
        if (stream_index < 0)
        {
                fprintf(stderr, "Could not find a video stream in the input file\n");
                avformat_close_input(&fmt_ctx);
                return -1;
        }

        AVStream *video_stream = fmt_ctx->streams[stream_index];
        if (video_stream->disposition & AV_DISPOSITION_ATTACHED_PIC)
        {

                AVPacket *pkt = &video_stream->attached_pic;

                FILE *file = fopen(coverFilePath, "wb");
                if (!file)
                {
                        fprintf(stderr, "Could not open output file '%s'\n", coverFilePath);
                        avformat_close_input(&fmt_ctx);
                        return -1;
                }
                fwrite(pkt->data, 1, pkt->size, file);
                fclose(file);

                avformat_close_input(&fmt_ctx);
                return 1;
        }
        else
        {
                avformat_close_input(&fmt_ctx);
                return -1;
        }

        avformat_close_input(&fmt_ctx);

        return 0;
}

static guint track_counter = 0;

// Generate a new track ID
gchar *generateTrackId()
{
        gchar *trackId = g_strdup_printf("/org/kew/tracklist/track%d", track_counter);
        track_counter++;
        return trackId;
}

void loadColor(SongData *songdata)
{
        getCoverColor(songdata->cover, &(songdata->red), &(songdata->green), &(songdata->blue));
}

void loadMetaData(SongData *songdata)
{
        char path[MAXPATHLEN];

        songdata->metadata = malloc(sizeof(TagSettings));
        generateTempFilePath(songdata->filePath, songdata->coverArtPath, "cover", ".jpg");
        int res = extractTags(songdata->filePath, songdata->metadata, &songdata->duration, songdata->coverArtPath);

        if (res < 0)
        {
                getDirectoryFromPath(songdata->filePath, path);
                char *tmp = NULL;
                off_t size = 0;
                tmp = findLargestImageFile(path, tmp, &size);

                if (tmp != NULL)
                        c_strcpy(songdata->coverArtPath, sizeof(songdata->coverArtPath), tmp);
                else
                        c_strcpy(songdata->coverArtPath, sizeof(songdata->coverArtPath), "q");
        }
        else
        {
                addToCache(tempCache, songdata->coverArtPath);
        }

        songdata->cover = getBitmap(songdata->coverArtPath);
}

SongData *loadSongData(char *filePath)
{
        SongData *songdata = NULL;
        songdata = malloc(sizeof(SongData));
        songdata->trackId = generateTrackId();
        songdata->hasErrors = false;
        c_strcpy(songdata->filePath, sizeof(songdata->filePath), "");
        c_strcpy(songdata->coverArtPath, sizeof(songdata->coverArtPath), "");
        songdata->red = NULL;
        songdata->green = NULL;
        songdata->blue = NULL;
        songdata->metadata = NULL;
        songdata->cover = NULL;
        songdata->duration = 0.0;
        c_strcpy(songdata->filePath, sizeof(songdata->filePath), filePath);       
        loadMetaData(songdata);
        loadColor(songdata);
        return songdata;
}

void unloadSongData(SongData **songdata)
{
        if (*songdata == NULL)
                return;

        SongData *data = *songdata;

        if (data->cover != NULL)
        {
                FreeImage_Unload(data->cover);
                data->cover = NULL;
        }

        if (existsInCache(tempCache, data->coverArtPath))
        {
                deleteFile(data->coverArtPath);
        }

        free(data->red);
        free(data->green);
        free(data->blue);
        free(data->metadata);
        free(data->trackId);

        data->cover = NULL;
        data->red = NULL;
        data->green = NULL;
        data->blue = NULL;
        data->metadata = NULL;

        data->trackId = NULL;

        free(*songdata);
        *songdata = NULL;
}
