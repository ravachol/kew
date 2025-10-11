#ifndef LYRICS_H
#define LYRICS_H

#include <stddef.h>
#include <stdbool.h>


typedef struct {
    double timestamp;
    char* text;
} LyricLine;
typedef struct {
    LyricLine* lines;
    size_t count;
} Lyrics;

/**
 * @brief 
 *
 * @param music_file_path 
 * @return 
 */
Lyrics *load_lyrics(const char *music_file_path);

/**
 * @brief 
 *
 * @param lyrics 
 */
void free_lyrics(Lyrics *lyrics);

/**
 * @brief 
 *
 * @param lyrics 
 * @param elapsed_seconds 
 * @return 
 */
const char *get_current_lyric_line(const Lyrics *lyrics, double elapsed_seconds);

#endif // LYRICS_H