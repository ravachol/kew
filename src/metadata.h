#ifndef METADATA_H
#define METADATA_H
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stringfunc.h"
#include "settings.h"

#ifndef KEYVALUEPAIR_STRUCT
#define KEYVALUEPAIR_STRUCT

typedef struct
{
    char *key;
    char *value;
} KeyValuePair;

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

TagSettings construct_tag_settings(KeyValuePair *pairs, int count);

int extract_tags(const char *input_file, const char *output_file);

void print_metadata(const char *file_path);

TagSettings getMetadata(const char *file_path);

#endif