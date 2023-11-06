#include "metadata.h"
#include "cache.h"
/*

metadata.c

 Functions for extracting tags from audio metadata.
 
*/
void removeTagPrefix(char *value)
{
        char *colon_pos = strchr(value, ':');
        if (colon_pos)
        {
                // Remove the tag prefix by shifting the characters
                memmove(value, colon_pos + 1, strlen(colon_pos));
        }
}

void turnFilePathIntoTitle(const char *filePath, char *title)
{
        char *lastSlash = strrchr(filePath, '/');
        char *lastDot = strrchr(filePath, '.');
        if (lastSlash != NULL && lastDot != NULL && lastDot > lastSlash)
        {
                snprintf(title, sizeof(title), "%s", lastSlash + 1);
                memcpy(title, lastSlash + 1, lastDot - lastSlash - 1); // Copy up to dst_size - 1 bytes
                title[lastDot - lastSlash - 1] = '\0';
                trim(title);
        }
}

int extractTags(const char *input_file, TagSettings *tag_settings)
{
        char command[1024];

        char *escapedInputFilePath = escapeFilePath(input_file);
        snprintf(command, sizeof(command), "ffprobe -show_entries format_tags -of default=noprint_wrappers=1:nokey=0 \"%s\"", escapedInputFilePath);

        free(escapedInputFilePath);

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

        if (strlen(tag_settings->title) <= 0)
        {
                char title[MAXPATHLEN];
                turnFilePathIntoTitle(input_file, title);
                snprintf(tag_settings->title, sizeof(tag_settings->title), "%s", title);
        }
        // Close the pipe
        pclose(pipe);

        return 0;
}