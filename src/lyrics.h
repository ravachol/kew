// lyrics.h
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

// In header
const char *getLyricsLine(const Lyrics *lyrics, double elapsedSeconds);

#ifdef __cplusplus
}
#endif

#endif // LYRICS_H

