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
extern "C"
{
#endif

#include "common/appstate.h"

int extractTags(const char *input_file, TagSettings *tag_settings,
                        double *duration, const char *coverFilePath, Lyrics **lyrics);

#ifdef __cplusplus
}
#endif

#endif // TAGLIB_WRAPPER_H
