/**
 * @file taglibwrapper.h
 * @brief C++ wrapper around TagLib for metadata extraction.
 *
 * Provides a simplified interface for reading song metadata such as
 * title, artist, album and embedded artwork using the TagLib library.
 * Exposes a C-compatible API for integration with the main C codebase.
 */

#ifndef TAGLIB_WRAPPER_H
#define TAGLIB_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "common/appstate.h"

/**
 * @brief Extracts metadata tags, duration, and cover art from an audio file.
 *
 * This function parses an audio file to extract its metadata (e.g., title,
 * artist, album, year), audio duration, and cover art. It supports various
 * audio formats including MP3, FLAC, OGG, Opus, and WAV. The function also
 * retrieves replay gain information and synchronized or unsynchronized lyrics
 * (if available).
 *
 * The extracted metadata is stored in the provided `tag_settings` structure,
 * the duration is stored in the `duration` pointer, and any lyrics are stored
 * in the `lyrics` pointer.
 *
 * The function performs the following tasks:
 * - Extracts title, artist, album, and year.
 * - Retrieves replay gain information (track and album gains).
 * - Loads lyrics (if available) from multiple formats (e.g., SYLT, USLT, Vorbis).
 * - Extracts the audio file's duration.
 * - Attempts to extract cover art based on the file's format.
 *
 * @param input_file Path to the audio file from which to extract metadata.
 * @param tag_settings A pointer to a TagSettings structure to store extracted
 *                     metadata (title, artist, album, replay gain, etc.).
 * @param duration A pointer to store the extracted audio duration in seconds.
 * @param cover_file_path The path where the extracted cover art should be saved.
 * @param lyrics A pointer to a Lyrics structure that will be populated with
 *               any lyrics found in the audio file.
 *
 * @return 0 if successful (cover art extracted), -1 if cover art extraction
 *         failed, -2 if the file is invalid or could not be parsed.
 *
 * @note This function relies on the TagLib library to parse audio files and
 *       extract metadata. It also supports cover art extraction for various
 *       file formats (MP3, FLAC, OGG, etc.).
 *
 * @warning If the input file is invalid, the function may not correctly
 *          initialize the tag settings, and fallback behavior will be applied.
 */
int extractTags(const char *input_file, TagSettings *tag_settings,
                double *duration, const char *cover_file_path, Lyrics **lyrics);

#ifdef __cplusplus
}
#endif

#endif // TAGLIB_WRAPPER_H
