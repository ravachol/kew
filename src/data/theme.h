/**
 * @file theme.h
 * @brief Loads themes.
 *
 * Loads themes, and copies the themes to config dir.
 */

#ifndef PIXELDATA_STRUCT
#define PIXELDATA_STRUCT

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct
{
        unsigned char r;
        unsigned char g;
        unsigned char b;
} PixelData;
#endif

#ifndef THEME_STRUCT
#define THEME_STRUCT

typedef enum
{
        COLOR_TYPE_RGB,
        COLOR_TYPE_ANSI
} ColorType;

typedef struct
{
        ColorType type;
        union
        {
                PixelData rgb;
                int8_t ansiIndex; // -1 to 15 for 16 colors + -1 = foreground
        };
} ColorValue;

typedef struct
{
        char theme_name[NAME_MAX];
        char theme_author[NAME_MAX];
        ColorValue accent;
        ColorValue text;
        ColorValue textDim;
        ColorValue textMuted;
        ColorValue logo;
        ColorValue header;
        ColorValue footer;
        ColorValue help;
        ColorValue link;
        ColorValue nowplaying;
        ColorValue playlist_rownum;
        ColorValue playlist_title;
        ColorValue playlist_playing;
        ColorValue trackview_title;
        ColorValue trackview_artist;
        ColorValue trackview_album;
        ColorValue trackview_year;
        ColorValue trackview_time;
        ColorValue trackview_visualizer;
        ColorValue trackview_lyrics;
        ColorValue library_artist;
        ColorValue library_album;
        ColorValue library_track;
        ColorValue library_enqueued;
        ColorValue library_playing;
        ColorValue search_label;
        ColorValue search_query;
        ColorValue search_result;
        ColorValue search_enqueued;
        ColorValue search_playing;
        ColorValue progress_filled;
        ColorValue progress_empty;
        ColorValue progress_elapsed;
        ColorValue progress_duration;
        ColorValue status_info;
        ColorValue status_warning;
        ColorValue status_error;
        ColorValue status_success;
} Theme;
#endif

int loadThemeFromFile(const char *themesDir, const char *filename, Theme *currentTheme);
bool ensureDefaultThemes(void);
