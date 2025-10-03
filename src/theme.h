#ifndef PIXELDATA_STRUCT
#define PIXELDATA_STRUCT

#include <limits.h>
typedef struct
{
        unsigned char r;
        unsigned char g;
        unsigned char b;
} PixelData;
#endif

#ifndef THEME_STRUCT
#define THEME_STRUCT

typedef struct
{
        char theme_name[NAME_MAX];
        char theme_author[NAME_MAX];
        PixelData accent;
        PixelData text;
        PixelData textDim;
        PixelData textMuted;
        PixelData logo;
        PixelData header;
        PixelData footer;
        PixelData help;
        PixelData link;
        PixelData nowplaying;
        PixelData playlist_rownum;
        PixelData playlist_title;
        PixelData playlist_playing;
        PixelData trackview_title;
        PixelData trackview_artist;
        PixelData trackview_album;
        PixelData trackview_year;
        PixelData trackview_time;
        PixelData trackview_visualizer;
        PixelData library_artist;
        PixelData library_album;
        PixelData library_track;
        PixelData library_enqueued;
        PixelData library_playing;
        PixelData search_label;
        PixelData search_query;
        PixelData search_result;
        PixelData search_enqueued;
        PixelData progress_filled;
        PixelData progress_empty;
        PixelData progress_elapsed;
        PixelData progress_duration;
        PixelData status_info;
        PixelData status_warning;
        PixelData status_error;
        PixelData status_success;
} Theme;
#endif

int loadTheme(const char *themesDir, const char *filename, Theme *currentTheme);
