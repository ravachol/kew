/**
 * @file songdatatype.h
 * @brief Decoders.
 *
 * Contains SongData type and related types.
 */

#ifndef SONGDATATYPE_H
#define SONGDATATYPE_H

#include <gio/gio.h>
#include <stdbool.h>
#include <stdint.h>

#define METADATA_MAX_LENGTH 256

#define SONG_MAGIC 0x534F4E47 // "SONG"

/**
 * @brief Stores metadata tags extracted from audio files.
 */
typedef struct
{
        char title[METADATA_MAX_LENGTH];
        char artist[METADATA_MAX_LENGTH];
        char album_artist[METADATA_MAX_LENGTH];
        char album[METADATA_MAX_LENGTH];
        char date[METADATA_MAX_LENGTH];
        double replaygainTrack;
        double replaygainAlbum;
} TagSettings;

typedef struct {
        double timestamp;
        char *text;
} LyricsLine;

typedef struct {
        LyricsLine *lines;
        size_t count;
        int max_length;
        int isTimed;
} Lyrics;

/**
 * @brief Holds decoded audio track data and metadata.
 */
typedef struct
{
        int magic;
        gchar *track_id;
        char file_path[PATH_MAX];
        char cover_art_path[PATH_MAX];

        int red;
        int green;
        int blue;

        TagSettings *metadata;

        unsigned char *cover;
        int avg_bit_rate;
        int coverWidth;
        int coverHeight;
        double duration;

        bool hasErrors;

        Lyrics *lyrics;
} SongData;

#endif
