
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

#include "songdatatype.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parses a slice of a lyric line for a timestamp that
 * is placed after the openerChar
 * 
 * @param lyricSlice pointer to the beginning of the lyric slice that should be parsed
 * 
 * @param timestamp pointer to a double value where the pased timestamp will be stored
 * 
 * @param openerChar a single character which opens the timestamp container
 *
 * @return Pointer to the character after the last parsed character
 */
char* parseTimestamp(char* lyricSlice, double* timestamp, char openerChar);

/**
 * Parses all present karaoke timestamps in a lyric line,
 * as they would be found in an Endhanced LRC. Also strips them from
 * the text
 * 
 * @param ptr pointer to the beginning of the lyric slice that should be parsed
 * 
 * @param lyrics pointer to the Lyrics struct to store the timestamps in.
 * 
 * @param firstStamp the first timestamp in the line
 *
 * @return number of timestamps written
 */
int parseKaraokeLine(char* ptr, Lyrics* lyrics, double firstStamp);

/**
 * Parses all present timestamps in a lyric line, and removes them from
 * the text.
 * Includes the first, whole-line timestamp and word-specific
 * timestamps
 * 
 * @param line pointer to the beginning of the lyric line that should be parsed
 * 
 * @param lyrics pointer to the Lyrics struct to store the parsed data in.
 * 
 * @param lyricsCapacity pointer to a size_t that stores the capacity
 *                       of the Lyrics struct pointed to by lyrics
 *
 * @return number of timestamps written
 */
int parseTimedLyricsLine(char* line, Lyrics* lyrics, size_t* lyricsCapacity);

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
Lyrics *loadLyricsFromLRC(const char *path,SongData *songdata);

/**
 * Releases all allocated LyricLines in a Lyrics struct,
 * as well as their text buffers
 *
 * @param lyrics Pointer to the Lyrics structure to operate on
 */
void freeLyricLines(Lyrics *lyrics);

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
