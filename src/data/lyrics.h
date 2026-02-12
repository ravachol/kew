
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
        int max_length;
        int isTimed;
} Lyrics;

/**
 * Loads lyrics from an LRC file corresponding to the given path.
 *
 * Replaces the file extension of @p path with ".lrc" and attempts
 * to open the resulting file. Automatically detects whether the
 * file contains timestamped lyrics and parses it accordingly.
 *
 * @param path Path to the original media file (used to derive the .lrc path)
 *
 * @return Pointer to a newly allocated Lyrics structure on success,
 *         or nullptr if the file could not be opened, parsed,
 *         or memory allocation failed. The returned structure
 *         must be freed with freeLyrics().
 */
Lyrics *loadLyricsFromLRC(const char *path);

/**
 * Frees a Lyrics structure and all associated memory.
 *
 * Releases all allocated lyric lines, their text buffers,
 * and the Lyrics container itself.
 *
 * @param lyrics Pointer to the Lyrics structure to free
 */
void freeLyrics(Lyrics *lyrics);

#ifdef __cplusplus
}
#endif

#endif // LYRICS_H
