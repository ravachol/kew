#ifndef LYRICS_H
#define LYRICS_H

#include <stdbool.h>
#include <stddef.h>

typedef struct
{
        double timestamp;
        char *text;
} LyricsLine;

typedef struct
{
        LyricsLine *lines;
        size_t count;
        int maxLength;
} Lyrics;

Lyrics *loadLyrics(const char *music_file_path);

void freeLyrics(Lyrics *lyrics);

char *getLyricsLine(Lyrics *lyrics, double elapsedSeconds);

#endif // LYRICS_H
