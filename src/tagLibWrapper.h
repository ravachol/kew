// taglib_wrapper.h
#ifndef TAGLIB_WRAPPER_H
#define TAGLIB_WRAPPER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "utils.h"

#ifndef TAGSETTINGS_STRUCT
#define METADATA_MAX_LENGTH 256
#define TAGSETTINGS_STRUCT
        typedef struct
        {
                char title[METADATA_MAX_LENGTH];
                char artist[METADATA_MAX_LENGTH];
                char album_artist[METADATA_MAX_LENGTH];
                char album[METADATA_MAX_LENGTH];
                char date[METADATA_MAX_LENGTH];
        } TagSettings;
#endif
        int extractTags(const char *input_file, TagSettings *tag_settings, double *duration, const char *coverFilePath);

#ifdef __cplusplus
}
#endif

#endif // TAGLIB_WRAPPER_H
