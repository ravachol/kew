#include "metadata.h"
#include "cache.h"

void removeTagPrefix(char *value)
{
    // Find the first occurrence of ':' in the value
    char *colon_pos = strchr(value, ':');
    if (colon_pos)
    {
        // Remove the tag prefix by shifting the characters
        memmove(value, colon_pos + 1, strlen(colon_pos));
    }
}

int extractTags(const char *input_file, TagSettings *tag_settings)
{
    char command[1024];
    snprintf(command, sizeof(command), "ffprobe -show_entries format_tags -of default=noprint_wrappers=1:nokey=0 \"%s\"", input_file);

    memset(tag_settings->title, 0, sizeof(tag_settings->title));
    memset(tag_settings->artist, 0, sizeof(tag_settings->artist));
    memset(tag_settings->album_artist, 0, sizeof(tag_settings->album_artist));
    memset(tag_settings->album, 0, sizeof(tag_settings->album));
    memset(tag_settings->date, 0, sizeof(tag_settings->date));   

    // Open the pipe to read the output of the ffprobe command
    FILE *pipe = popen(command, "r");
    if (!pipe)
    {
        fprintf(stderr, "Error executing ffprobe command.\n");
        return 1;
    }

    char line[512];
    while (fgets(line, sizeof(line), pipe))
    {
        // Extract the key and value from each line
        char *key = strtok(line, "=");
        char *value = strtok(NULL, "=");
        if (key && value)
        {
            // Remove newline character from the end of the value
            size_t value_len = strlen(value);
            if (value[value_len - 1] == '\n')
                value[value_len - 1] = '\0';

            // Remove the tag prefix from the value if it exists
            removeTagPrefix(key);

            // Assign the value to the corresponding field in the TagSettings structure
            if (strcasecmp(key, "title") == 0)
                snprintf(tag_settings->title, sizeof(tag_settings->title), "%s", value);
            else if (strcasecmp(key, "artist") == 0)
                snprintf(tag_settings->artist, sizeof(tag_settings->artist), "%s", value);
            else if (strcasecmp(key, "album_artist") == 0)
                snprintf(tag_settings->album_artist, sizeof(tag_settings->album_artist), "%s", value);
            else if (strcasecmp(key, "album") == 0)
                snprintf(tag_settings->album, sizeof(tag_settings->album), "%s", value);
            else if (strcasecmp(key, "date") == 0)
                snprintf(tag_settings->date, sizeof(tag_settings->date), "%s", value);
        }
    }

    // Close the pipe
    pclose(pipe);

    return 0;
}