

/**
 * @file theme.c
 * @brief Loads themes.
 *
 * Loads themes, and copies the themes to config dir.
 */

#include "theme.h"

#include "common/appstate.h"
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

int hex_to_pixel(const char *hex, PixelData *result)
{
        if (!hex || strlen(hex) != 7 || hex[0] != '#')
                return -1;

        if (hex[0] == '#')
                hex++; // skip #

        if (strlen(hex) == 6) {
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

void trim_whitespace(char *str)
{
        while (isspace((unsigned char)*str))
                str++;

        char *end = str + strlen(str) - 1;
        while (end > str && isspace((unsigned char)*end))
                *end-- = '\0';

        memmove(str, str, strlen(str) + 1);
}

void remove_comment(char *str)
{
        char *p = str;
        while (*p) {
                if (*p == '#') {
                        // If previous char is whitespace, treat as comment
                        // start
                        if (p == str || isspace((unsigned char)*(p - 1))) {
                                *p = '\0';
                                break;
                        }
                }
                p++;
        }
}

// Parse hex color safely (e.g. #aabbcc)
int parse_hex_color(const char *hex, PixelData *out)
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

int parse_color_value(const char *value, ColorValue *out)
{
        if (!value || !out)
                return 0;

        // Check if it's hex (#RRGGBB)
        if (value[0] == '#') {
                unsigned int r, g, b;
                if (sscanf(value, "#%02x%02x%02x", &r, &g, &b) != 3) {
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
        if (errno || endptr == value || *endptr != '\0') {
                return 0; // invalid number
        }

        if (index < -1 || index > 15) {
                return 0; // out of range for 16-color ANSI
        }

        out->type = COLOR_TYPE_ANSI;
        out->ansiIndex = (int8_t)index;
        return 1;
}

int load_theme_from_file(const char *themes_dir, const char *filename, Theme *current_theme)
{
        memset(current_theme, 0, sizeof(Theme));

        if (!themes_dir || !filename) {
                fprintf(stderr, "Theme directory or filename is NULL.\n");
                set_error_message("Theme directory or filename is NULL.");
                return 0;
        }

        char path[512];
        if (snprintf(path, sizeof(path), "%s/%s", themes_dir, filename) >=
            (int)sizeof(path)) {
                fprintf(stderr, "Theme path too long.\n");
                return 0;
        }

        FILE *file = fopen(path, "r");
        if (!file) {
                fprintf(stderr, "Failed to open theme file.\n");
                set_error_message("Failed to open theme file.");
                return 0;
        }

        // Map of all known keys to Theme fields
        ThemeMapping mappings[] = {
            {"accent", &current_theme->accent},
            {"text", &current_theme->text},
            {"textDim", &current_theme->textDim},
            {"textMuted", &current_theme->textMuted},
            {"logo", &current_theme->logo},
            {"header", &current_theme->header},
            {"footer", &current_theme->footer},
            {"help", &current_theme->help},
            {"link", &current_theme->link},
            {"nowplaying", &current_theme->nowplaying},
            {"playlist_rownum", &current_theme->playlist_rownum},
            {"playlist_title", &current_theme->playlist_title},
            {"playlist_playing", &current_theme->playlist_playing},
            {"trackview_title", &current_theme->trackview_title},
            {"trackview_artist", &current_theme->trackview_artist},
            {"trackview_album", &current_theme->trackview_album},
            {"trackview_year", &current_theme->trackview_year},
            {"trackview_time", &current_theme->trackview_time},
            {"trackview_visualizer", &current_theme->trackview_visualizer},
            {"trackview_lyrics", &current_theme->trackview_lyrics},
            {"library_artist", &current_theme->library_artist},
            {"library_album", &current_theme->library_album},
            {"library_track", &current_theme->library_track},
            {"library_enqueued", &current_theme->library_enqueued},
            {"library_playing", &current_theme->library_playing},
            {"search_label", &current_theme->search_label},
            {"search_query", &current_theme->search_query},
            {"search_result", &current_theme->search_result},
            {"search_enqueued", &current_theme->search_enqueued},
            {"search_playing", &current_theme->search_playing},
            {"progress_filled", &current_theme->progress_filled},
            {"progress_elapsed", &current_theme->progress_elapsed},
            {"progress_empty", &current_theme->progress_empty},
            {"progress_duration", &current_theme->progress_duration},
            {"status_info", &current_theme->status_info},
            {"status_warning", &current_theme->status_warning},
            {"status_error", &current_theme->status_error},
            {"status_success", &current_theme->status_success}};

        const size_t mapping_count = sizeof(mappings) / sizeof(ThemeMapping);

        AppState *state = get_app_state();

        ColorValue default_color;
        default_color.type = COLOR_TYPE_RGB;
        default_color.rgb = state->uiSettings.defaultColorRGB;

        for (size_t i = 0; i < mapping_count; ++i) {
                *(mappings[i].field) = default_color;
        }

        char line[512];
        int line_num = 0;
        int found = 0;

        while (fgets(line, sizeof(line), file)) {
                line_num++;

                remove_comment(line);
                trim_whitespace(line);

                if (strlen(line) == 0 || line[0] == '[')
                        continue; // skip empty or section headers

                char *eq = strchr(line, '=');
                if (!eq) {
                        continue;
                }

                *eq = '\0';
                char *key = line;
                char *value = eq + 1;

                trim_whitespace(key);
                trim_whitespace(value);

                // Replace dots with underscores
                for (char *c = key; *c; c++) {
                        if (*c == '.')
                                *c = '_';
                }

                for (size_t i = 0; i < mapping_count; ++i) {
                        if (strcmp(key, "name") == 0) {
                                // Copy theme name safely
                                strncpy(current_theme->theme_name, value,
                                        sizeof(current_theme->theme_name) - 1);
                                current_theme->theme_name
                                    [sizeof(current_theme->theme_name) - 1] =
                                    '\0';
                                found = 1;
                                break;
                        }
                        if (strcmp(key, "author") == 0) {
                                // Copy theme name safely
                                strncpy(current_theme->theme_author, value,
                                        sizeof(current_theme->theme_author) - 1);
                                current_theme->theme_author
                                    [sizeof(current_theme->theme_author) - 1] =
                                    '\0';
                                found = 1;
                                break;
                        } else if (strcmp(key, mappings[i].key) == 0) {
                                ColorValue color;

                                if (!parse_color_value(value, &color)) {
                                        fprintf(stderr,
                                                "Invalid color value at line "
                                                "%d: %s\n",
                                                line_num, value);
                                } else {
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

bool ensure_default_themes(void)
{
    bool copied = false;

    char *config_path = get_config_path();
    if (!config_path)
        return false;

    char themes_path[PATH_MAX];
    if (snprintf(themes_path, sizeof(themes_path), "%s/themes", config_path) >= (int)sizeof(themes_path)) {
        free(config_path);
        return false;
    }

    struct stat st;
    if (stat(themes_path, &st) == -1) {
        // Directory doesn't exist → create it
        if (mkdir(themes_path, 0755) == -1) {
            free(config_path);
            return false;
        }
    }

    const char *system_themes = PREFIX "/share/kew/themes";
    DIR *dir = opendir(system_themes);
    if (!dir) {
        free(config_path);
        return false;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Only copy real files that look like themes
        if (entry->d_type == DT_REG &&
            (strstr(entry->d_name, ".theme") || strstr(entry->d_name, ".txt"))) {

            char src[PATH_MAX], dst[PATH_MAX];

            if (snprintf(src, sizeof(src), "%s/%s", system_themes, entry->d_name) >= (int)sizeof(src))
                continue;
            if (snprintf(dst, sizeof(dst), "%s/%s", themes_path, entry->d_name) >= (int)sizeof(dst))
                continue;

            bool need_copy = true;

            // Check modification time
            struct stat src_st, dst_st;
            if (stat(src, &src_st) == 0 && stat(dst, &dst_st) == 0) {
                if (difftime(src_st.st_mtime, dst_st.st_mtime) <= 0) {
                    // Destination is newer or same → no need to copy
                    need_copy = false;
                }
            }

            if (need_copy) {
                if (copy_file(src, dst))
                    copied = true;
            }
        }
    }

    closedir(dir);
    free(config_path);
    return copied;
}
