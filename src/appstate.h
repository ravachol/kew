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
        int mainColor;                                  // Main terminal color, when using config colors
        int titleColor;                                 // Color of the title, when using config colors
        int artistColor;                                // Artist color, when using config colors
        int enqueuedColor;                              // Color of enqueued files, when using config colors
        PixelData color;                                // The current color, when using album derived colors
        bool useConfigColors;                           // Use colors stored in config file or use an album derived color
        bool coverEnabled;                              // Show covers or not
        bool uiEnabled;                                 // Show ui or not
        bool coverAnsi;                                 // Show chafa cover (picture perfect in the right terminal), or ascii/ansi typ cover
        bool visualizerEnabled;                         // Show spectrum visualizer
        bool nerdFontsEnabled;                          // Show nerd font icons
        bool hideLogo;                                  // No kew text at top
        bool hideHelp;                                  // No help text at top
        bool allowNotifications;                        // Send desktop notifications or not
        int visualizerHeight;                           // Height in characters of the spectrum visualizer
        int cacheLibrary;                               // Cache the library or not
        bool quitAfterStopping;                         // Exit kew when the music stops or not
        time_t lastTimeAppRan;                          // When did this app run last, used for updating the cached library if it has been modified since that time         
} UISettings;

typedef struct
{
        int chosenNodeId;                               // The id of the tree node that is chosen in library view        
        bool allowChooseSongs;                          // In library view, has the user entered a folder that contains songs                               
        bool openedSubDir;                              // Opening a directory in an open directory.        
        int numSongsAboveSubDir;                        // How many rows do we need to jump up if we close the parent directory and open one within
        int numDirectoryTreeEntries;                    // The number of entries in directory tree in library view
        int numProgressBars;                            // The number of progress dots at the bottom of track view
        volatile sig_atomic_t resizeFlag;               // Is the user resizing the terminal window
        bool resetPlaylistDisplay;                      // Should the playlist be reset, ie drawn starting from playing song                 
        bool doNotifyMPRISSwitched;                     // Emit mpris song switched signal        
        bool doNotifyMPRISPlaying;                      // Emit mpris music is playing signal
} UIState;

typedef struct
{
        Cache *tempCache;                               // Cache for temporary files
        ViewState currentView;                          // The current view (playlist, library, track) that kew is on
        UIState uiState;
        UISettings uiSettings;
} AppState;

static const unsigned char defaultColor = 150;

#endif
