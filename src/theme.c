#include "theme.h"
#include "common.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Struct internal untuk memetakan nama kunci di file tema ke field di struct Theme
typedef struct
{
    const char *key;
    ColorValue *field;
} ThemeMapping;


static void setDefaultColor(ColorValue *color, uint8_t r, uint8_t g, uint8_t b) {
    color->type = COLOR_TYPE_RGB;
    color->rgb.r = r;
    color->rgb.g = g;
    color->rgb.b = b;
}

void initTheme(Theme *theme)
{
    memset(theme, 0, sizeof(Theme));
    strcpy(theme->theme_name, "default");
    strcpy(theme->theme_author, "kew");

    // Default colors
    setDefaultColor(&theme->accent, 255, 107, 107);
    setDefaultColor(&theme->text, 220, 220, 220);
    setDefaultColor(&theme->textDim, 150, 150, 150);
    setDefaultColor(&theme->textMuted, 100, 100, 100);
    setDefaultColor(&theme->logo, 255, 107, 107);
    setDefaultColor(&theme->header, 120, 120, 120);
    setDefaultColor(&theme->footer, 120, 120, 120);
    setDefaultColor(&theme->help, 180, 180, 180);
    setDefaultColor(&theme->link, 107, 155, 255);
    setDefaultColor(&theme->nowplaying, 255, 107, 107);
    setDefaultColor(&theme->playlist_rownum, 100, 100, 100);
    setDefaultColor(&theme->playlist_title, 220, 220, 220);
    setDefaultColor(&theme->playlist_playing, 255, 107, 107);
    setDefaultColor(&theme->trackview_title, 255, 255, 255);
    setDefaultColor(&theme->trackview_artist, 200, 200, 200);
    setDefaultColor(&theme->trackview_album, 180, 180, 180);
    setDefaultColor(&theme->trackview_year, 150, 150, 150);
    setDefaultColor(&theme->trackview_time, 150, 150, 150);
    setDefaultColor(&theme->trackview_visualizer, 255, 107, 107);
    setDefaultColor(&theme->trackview_lyrics, 200, 200, 200); // Warna default untuk lirik
    setDefaultColor(&theme->library_artist, 220, 220, 220);
    setDefaultColor(&theme->library_album, 200, 200, 200);
    setDefaultColor(&theme->library_track, 180, 180, 180);
    setDefaultColor(&theme->library_enqueued, 107, 255, 155);
    setDefaultColor(&theme->library_playing, 255, 107, 107);
    setDefaultColor(&theme->search_label, 150, 150, 150);
    setDefaultColor(&theme->search_query, 255, 255, 255);
    setDefaultColor(&theme->search_result, 220, 220, 220);
    setDefaultColor(&theme->search_enqueued, 107, 255, 155);
    setDefaultColor(&theme->search_playing, 255, 107, 107);
    setDefaultColor(&theme->progress_filled, 255, 107, 107);
    setDefaultColor(&theme->progress_elapsed, 220, 220, 220);
    setDefaultColor(&theme->progress_empty, 80, 80, 80);
    setDefaultColor(&theme->progress_duration, 150, 150, 150);
    setDefaultColor(&theme->status_info, 107, 155, 255);
    setDefaultColor(&theme->status_warning, 255, 255, 107);
    setDefaultColor(&theme->status_error, 255, 50, 50);
    setDefaultColor(&theme->status_success, 107, 255, 155);
}


void freeTheme(Theme *theme) {
    (void)theme; 
}


const char* getColorString(const ColorValue *color, char *buffer, size_t buf_size) {
    if (color->type == COLOR_TYPE_RGB) {
        snprintf(buffer, buf_size, "\033[38;2;%d;%d;%dm", color->rgb.r, color->rgb.g, color->rgb.b);
    } else { // COLOR_TYPE_ANSI
        snprintf(buffer, buf_size, "\033[38;5;%dm", color->ansiIndex);
    }
    return buffer;
}


void trimWhitespace(char *str) {
    char *start = str;
    while (isspace((unsigned char)*start)) {
        start++;
    }
    char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) {
        *end-- = '\0';
    }
    memmove(str, start, strlen(start) + 1);
}

void removeComment(char *str) {
    char *p = strchr(str, '#');
    if (p) {
        *p = '\0';
    }
}

int parseColorValue(const char *value, ColorValue *out) {
    if (!value || !out) return 0;

    if (value[0] == '#') {
        unsigned int r, g, b;
        if (sscanf(value, "#%02x%02x%02x", &r, &g, &b) != 3) {
            return 0;
        }
        out->type = COLOR_TYPE_RGB;
        out->rgb.r = (uint8_t)r;
        out->rgb.g = (uint8_t)g;
        out->rgb.b = (uint8_t)b;
        return 1;
    }

    char *endptr = NULL;
    errno = 0;
    long index = strtol(value, &endptr, 10);
    if (errno || endptr == value || *endptr != '\0' || index < -1 || index > 255) {
        return 0;
    }
    out->type = COLOR_TYPE_ANSI;
    out->ansiIndex = (int8_t)index;
    return 1;
}

int loadThemeFromFile(const char *themesDir, const char *filename, Theme *currentTheme) {
    
    initTheme(currentTheme);

    if (!themesDir || !filename) {
        setErrorMessage("Theme directory or filename is NULL.");
        return 0;
    }

    char path[512];
    snprintf(path, sizeof(path), "%s/%s", themesDir, filename);

    FILE *file = fopen(path, "r");
    if (!file) {
        setErrorMessage("Failed to open theme file.");
        return 0;
    }

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
            {"status_success", &currentTheme->status_success}
    };
    const size_t mappingCount = sizeof(mappings) / sizeof(ThemeMapping);

    char line[512];
    int lineNum = 0;
    while (fgets(line, sizeof(line), file)) {
        lineNum++;
        removeComment(line);
        trimWhitespace(line);

        if (strlen(line) == 0) continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = line;
        char *value = eq + 1;
        trimWhitespace(key);
        trimWhitespace(value);

        for (char *c = key; *c; c++) {
            if (*c == '.') *c = '_';
        }

        bool key_found = false;
        if (strcmp(key, "name") == 0) {
            strncpy(currentTheme->theme_name, value, sizeof(currentTheme->theme_name) - 1);
            key_found = true;
        } else if (strcmp(key, "author") == 0) {
            strncpy(currentTheme->theme_author, value, sizeof(currentTheme->theme_author) - 1);
            key_found = true;
        } else {
            for (size_t i = 0; i < mappingCount; ++i) {
                if (strcmp(key, mappings[i].key) == 0) {
                    if (!parseColorValue(value, mappings[i].field)) {
                        fprintf(stderr, "Invalid color value at line %d: %s\n", lineNum, value);
                    }
                    key_found = true;
                    break;
                }
            }
        }
    }

    fclose(file);
    return 1;
}