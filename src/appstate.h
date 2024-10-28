#ifndef APPSTATE_H
#define APPSTATE_H

#include "cache.h"

#include <gio/gio.h>
#include <glib.h>
#ifdef USE_LIBNOTIFY
#include <libnotify/notify.h>
#endif
#include <sys/param.h>

#ifndef PIXELDATA_STRUCT
#define PIXELDATA_STRUCT

typedef struct
{
        unsigned char r;
        unsigned char g;
        unsigned char b;
} PixelData;
#endif

typedef enum
{
        SONG_VIEW,
        KEYBINDINGS_VIEW,
        PLAYLIST_VIEW,
        LIBRARY_VIEW,
        SEARCH_VIEW
} ViewState;

typedef struct
{
        int mainColor;
        int titleColor;
        int artistColor;
        int enqueuedColor;
        PixelData color;
        bool useProfileColors;
        bool coverEnabled;
        bool uiEnabled;
        bool coverAnsi;
        bool visualizerEnabled;
        bool useThemeColors;
        bool nerdFontsEnabled;
        bool hideLogo;
        bool hideHelp;
        bool allowNotifications;
        int visualizerHeight;
        int cacheLibrary;
        bool quitAfterStopping;
} UISettings;

typedef struct
{
        bool allowChooseSongs;
        int chosenLibRow;
        int chosenSearchResultRow;
        int chosenRow;
        int chosenNodeId;
        int chosenSong;
        int numDirectoryTreeEntries;
        int numProgressBars;
        volatile bool refresh;
        volatile sig_atomic_t resizeFlag;
        bool newUndisplayedSearch;
        bool resetPlaylistDisplay;
        struct timespec start_time;
        struct timespec lastInputTime;
#ifdef USE_LIBNOTIFY
        NotifyNotification *previous_notification;
#endif
        GDBusConnection *connection;
        GMainContext *global_main_context;
        bool doQuit;
        bool doNotifyMPRISSwitched;
        Cache *tempCache;
        time_t lastTimeAppRan;
        ViewState currentView;
} UIState;

typedef struct
{
        ViewState currentView;
        UIState uiState;
        UISettings uiSettings;
} AppState;

static const unsigned char defaultColor = 150;

#endif
