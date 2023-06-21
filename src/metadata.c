#include "metadata.h"
#include "cache.h"

TagSettings construct_tag_settings(KeyValuePair *pairs, int count)
{
    TagSettings settings;
    memset(&settings, 0, sizeof(settings));

    for (int i = 0; i < count; i++)
    {
        KeyValuePair *pair = &pairs[i];

        if (strcmp(stringToLower(pair->key), "title") == 0)
        {
            strncpy(settings.title, pair->value, sizeof(settings.title));
        }
        else if (strcmp(stringToLower(pair->key), "artist") == 0)
        {
            strncpy(settings.artist, pair->value, sizeof(settings.artist));
        }
        else if (strcmp(stringToLower(pair->key), "album") == 0)
        {
            strncpy(settings.album, pair->value, sizeof(settings.album));
        }
        else if (strcmp(stringToLower(pair->key), "date") == 0)
        {
            strncpy(settings.date, pair->value, sizeof(settings.date));
        }
    }

    return settings;
}

int extract_tags(const char *input_file, const char *output_file)
{
    // Open the input file
    AVFormatContext *format_ctx = NULL;
    if (avformat_open_input(&format_ctx, input_file, NULL, NULL) != 0)
    {
        fprintf(stderr, "Error opening the input file.\n");
        return 1;
    }

    // Retrieve the stream information
    if (avformat_find_stream_info(format_ctx, NULL) < 0)
    {
        fprintf(stderr, "Error finding stream information.\n");
        avformat_close_input(&format_ctx);
        return 1;
    }

    // Open the output file
    FILE *output_file_ptr = fopen(output_file, "w");
    if (!output_file_ptr)
    {
        fprintf(stderr, "Error opening the output file.\n");
        avformat_close_input(&format_ctx);
        return 1;
    }

    // Extract the metadata tags
    AVDictionaryEntry *tag = NULL;
    while ((tag = av_dict_get(format_ctx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
    {
        fprintf(output_file_ptr, "%s=%s\n", tag->key, tag->value);
    }

    // Close the files and free resources
    avformat_close_input(&format_ctx);
    fclose(output_file_ptr);

    addToCache(tempCache, output_file);
    return 0;
}

void print_metadata(const char *file_path)
{
    FILE *file = fopen(file_path, "r");
    if (file == NULL)
    {
        fprintf(stderr, "Error opening the file.\n");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file))
    {
        // Remove trailing newline character if present
        line[strcspn(line, "\n")] = '\0';

        char *delimiter = strchr(line, '=');
        if (delimiter != NULL)
        {
            *delimiter = '\0';
            char *key = line;
            char *value = delimiter + 1;

            printf("%s: %s\n", key, value);
        }
    }

    fclose(file);
}

TagSettings getMetadata(const char *file_path)
{
    int pair_count;
    KeyValuePair *pairs = readKeyValuePairs(file_path, &pair_count);
    TagSettings metadata = {};

    if (pairs == NULL)
    {
        return metadata;
    }
    metadata = construct_tag_settings(pairs, pair_count);
    freeKeyValuePairs(pairs, pair_count);
    return metadata;
}