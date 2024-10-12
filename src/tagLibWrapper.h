// taglib_wrapper.h
#ifndef TAGLIB_WRAPPER_H
#define TAGLIB_WRAPPER_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef TAGSETTINGS_STRUCT
#define TAGSETTINGS_STRUCT
        typedef struct
        {
                char title[256];
                char artist[256];
                char album_artist[256];
                char album[256];
                char date[256];
        } TagSettings;
#endif
        int extractTags(const char *input_file, TagSettings *tag_settings, double *duration, const char *coverFilePath);

#ifdef __cplusplus
}
#endif

#endif // TAGLIB_WRAPPER_H
