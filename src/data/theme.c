

/**
 * @file theme.c
 * @brief Loads themes.
 *
 * Loads themes, and copies the themes to config dir.
 */

#include "theme.h"

#include "common/common.h"

#include "utils/utils.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef PREFIX
#define PREFIX "/usr/local" // Fallback if not set in the makefile
#endif

typedef struct
{
        const char *key;
        ColorValue *field;
} ThemeMapping;

int hexToPixel(const char *hex, PixelData *result)
{
        if (!hex || strlen(hex) != 7 || hex[0] != '#')
                return -1;

        if (hex[0] == '#')
                hex++; // skip #

        if (strlen(hex) == 6)
        {
                char r[3], g[3], b[3];
                strncpy(r, hex, 2);
                r[2] = '\0';
                strncpy(g, hex + 2, 2);
                g[2] = '\0';
                strncpy(b, hex + 4, 2);
                b[2] = '\0';

                (*result).r = (unsigned char)strtol(r, NULL, 16);
                (*result).g = (unsigned char)strtol(g, NULL, 16);
                (*result).b = (unsigned char)strtol(b, NULL, 16);
        }
        return 0;
}

void trimWhitespace(char *str)
{
        while (isspace((unsigned char)*str))
                str++;

        char *end = str + strlen(str) - 1;
        while (end > str && isspace((unsigned char)*end))
                *end-- = '\0';

        memmove(str, str, strlen(str) + 1);
}

void removeComment(char *str)
{
        char *p = str;
        while (*p)
        {
                if (*p == '#')
                {
                        // If previous char is whitespace, treat as comment
                        // start
                        if (p == str || isspace((unsigned char)*(p - 1)))
                        {
                                *p = '\0';
                                break;
                        }
                }
                p++;
        }
}

// Parse hex color safely (e.g. #aabbcc)
int parseHexColor(const char *hex, PixelData *out)
{
        if (!hex || strlen(hex) != 7 || hex[0] != '#')
                return 0;

        unsigned int r, g, b;
        if (sscanf(hex + 1, "%02x%02x%02x", &r, &g, &b) != 3)
                return 0;

        out->r = (unsigned char)r;
        out->g = (unsigned char)g;
        out->b = (unsigned char)b;
        return 1;
}

int parseColorValue(const char *value, ColorValue *out)
{
        if (!value || !out)
                return 0;

        // Check if it's hex (#RRGGBB)
        if (value[0] == '#')
        {
                unsigned int r, g, b;
                if (sscanf(value, "#%02x%02x%02x", &r, &g, &b) != 3)
                {
                        return 0; // failed to parse hex
                }
                out->type = COLOR_TYPE_RGB;
                out->rgb.r = (uint8_t)r;
                out->rgb.g = (uint8_t)g;
                out->rgb.b = (uint8_t)b;
                return 1;
        }

        // Otherwise, try integer for ANSI index
        char *endptr = NULL;
        errno = 0;
        long index = strtol(value, &endptr, 10);
        if (errno || endptr == value || *endptr != '\0')
        {
                return 0; // invalid number
        }

        if (index < -1 || index > 15)
        {
                return 0; // out of range for 16-color ANSI
        }

        out->type = COLOR_TYPE_ANSI;
        out->ansiIndex = (int8_t)index;
        return 1;
}

int loadThemeFromFile(const char *themesDir, const char *filename, Theme *currentTheme)
{
        memset(currentTheme, 0, sizeof(Theme));

        if (!themesDir || !filename)
        {
                fprintf(stderr, "Theme directory or filename is NULL.\n");
                setErrorMessage("Theme directory or filename is NULL.");
                return 0;
        }

        char path[512];
        if (snprintf(path, sizeof(path), "%s/%s", themesDir, filename) >=
            (int)sizeof(path))
        {
                fprintf(stderr, "Theme path too long.\n");
                return 0;
        }

        FILE *file = fopen(path, "r");
        if (!file)
        {
                fprintf(stderr, "Failed to open theme file.\n");
                setErrorMessage("Failed to open theme file.");
                return 0;
        }

        // Map of all known keys to Theme fields
        ThemeMapping mappings[] = {
            {"accent", &currentTheme->accent},
            {"text", &currentTheme->text},
            {"textDim", &currentTheme->textDim},
            {"textMuted", &currentTheme->textMuted},
            {"logo", &currentTheme->logo},
            {"header", &currentTheme->header},
            {"footer", &currentTheme->footer},
            {"help", &currentTheme->help},
            {"link", &currentTheme->link},
            {"nowplaying", &currentTheme->nowplaying},
            {"playlist_rownum", &currentTheme->playlist_rownum},
            {"playlist_title", &currentTheme->playlist_title},
            {"playlist_playing", &currentTheme->playlist_playing},
            {"trackview_title", &currentTheme->trackview_title},
            {"trackview_artist", &currentTheme->trackview_artist},
            {"trackview_album", &currentTheme->trackview_album},
            {"trackview_year", &currentTheme->trackview_year},
            {"trackview_time", &currentTheme->trackview_time},
            {"trackview_visualizer", &currentTheme->trackview_visualizer},
            {"trackview_lyrics", &currentTheme->trackview_lyrics},
            {"library_artist", &currentTheme->library_artist},
            {"library_album", &currentTheme->library_album},
            {"library_track", &currentTheme->library_track},
            {"library_enqueued", &currentTheme->library_enqueued},
            {"library_playing", &currentTheme->library_playing},
            {"search_label", &currentTheme->search_label},
            {"search_query", &currentTheme->search_query},
            {"search_result", &currentTheme->search_result},
            {"search_enqueued", &currentTheme->search_enqueued},
            {"search_playing", &currentTheme->search_playing},
            {"progress_filled", &currentTheme->progress_filled},
            {"progress_elapsed", &currentTheme->progress_elapsed},
            {"progress_empty", &currentTheme->progress_empty},
            {"progress_duration", &currentTheme->progress_duration},
            {"status_info", &currentTheme->status_info},
            {"status_warning", &currentTheme->status_warning},
            {"status_error", &currentTheme->status_error},
            {"status_success", &currentTheme->status_success}};

        const size_t mappingCount = sizeof(mappings) / sizeof(ThemeMapping);

        char line[512];
        int lineNum = 0;
        int found = 0;

        while (fgets(line, sizeof(line), file))
        {
                lineNum++;

                removeComment(line);
                trimWhitespace(line);

                if (strlen(line) == 0 || line[0] == '[')
                        continue; // skip empty or section headers

                char *eq = strchr(line, '=');
                if (!eq)
                {
                        continue;
                }

                *eq = '\0';
                char *key = line;
                char *value = eq + 1;

                trimWhitespace(key);
                trimWhitespace(value);

                // Replace dots with underscores
                for (char *c = key; *c; c++)
                {
                        if (*c == '.')
                                *c = '_';
                }

                for (size_t i = 0; i < mappingCount; ++i)
                {
                        if (strcmp(key, "name") == 0)
                        {
                                // Copy theme name safely
                                strncpy(currentTheme->theme_name, value,
                                        sizeof(currentTheme->theme_name) - 1);
                                currentTheme->theme_name
                                    [sizeof(currentTheme->theme_name) - 1] =
                                    '\0';
                                found = 1;
                                break;
                        }
                        if (strcmp(key, "author") == 0)
                        {
                                // Copy theme name safely
                                strncpy(currentTheme->theme_author, value,
                                        sizeof(currentTheme->theme_author) - 1);
                                currentTheme->theme_author
                                    [sizeof(currentTheme->theme_author) - 1] =
                                    '\0';
                                found = 1;
                                break;
                        }
                        else if (strcmp(key, mappings[i].key) == 0)
                        {
                                ColorValue color;

                                if (!parseColorValue(value, &color))
                                {
                                        fprintf(stderr,
                                                "Invalid color value at line "
                                                "%d: %s\n",
                                                lineNum, value);
                                }
                                else
                                {
                                        *(mappings[i].field) = color;
                                        found = 1;
                                }
                                break;
                        }
                }
        }

        fclose(file);
        return found;
}

// Copies default themes to config dir if they aren't alread there
bool ensureDefaultThemes(void)
{
        bool copied = false;

        char *configPath = getConfigPath();
        if (!configPath)
                return false;

        char themesPath[MAXPATHLEN];
        if (snprintf(themesPath, sizeof(themesPath), "%s/themes", configPath) >=
            (int)sizeof(themesPath))
        {
                free(configPath);
                return false;
        }

        // Check if user themes directory already exists
        struct stat st;
        if (stat(themesPath, &st) == -1)
        {
                char *systemThemes = PREFIX "/share/kew/themes";
                DIR *dir = opendir(systemThemes);
                if (dir)
                {
                        struct dirent *entry;
                        bool needsDir = false;

                        while ((entry = readdir(dir)) != NULL)
                        {
                                if (entry->d_type == DT_REG &&
                                    (strstr(entry->d_name, ".theme") ||
                                     strstr(entry->d_name, ".txt")))
                                {
                                        // Found at least one theme — create dir if not yet created
                                        if (!needsDir)
                                        {
                                                if (mkdir(themesPath, 0755) == 0)
                                                        needsDir = true;
                                                else
                                                        break; // couldn't create directory
                                        }

                                        char src[MAXPATHLEN], dst[MAXPATHLEN];

                                        if (snprintf(src, sizeof(src), "%s/%s",
                                                     systemThemes, entry->d_name) >= (int)sizeof(src))
                                                continue;
                                        if (snprintf(dst, sizeof(dst), "%s/%s",
                                                     themesPath, entry->d_name) >= (int)sizeof(dst))
                                                continue;

                                        copyFile(src, dst);
                                        copied = true;
                                }
                        }
                        closedir(dir);
                }
        }

        free(configPath);
        return copied;
}
