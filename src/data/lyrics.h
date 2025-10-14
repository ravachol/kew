
/**
 * @file lyrics.h
 * @brief Lyrics fetching and parsing.
 *
 * Provides functions to load, cache lyrics from local files
 * or remote sources. Supports synchronized lyric display and fallback modes
 * when metadata or network access is unavailable.
 */


#ifndef LYRICS_H
#define LYRICS_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double timestamp;
    char *text;
} LyricsLine;

typedef struct {
    LyricsLine *lines;
    size_t count;
    int maxLength;
    int isTimed;
} Lyrics;

Lyrics *loadLyrics(const char *music_file_path);
void freeLyrics(Lyrics *lyrics);

#ifdef __cplusplus
}
#endif

#endif // LYRICS_H

