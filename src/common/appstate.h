/**
 * @file appstate.h
 * @brief Provides globally accessible state structs, getters and setters.
 *
 */

#ifndef APPSTATE_H
#define APPSTATE_H

#include "data/lyrics.h"
#include "data/playlist.h"
#include "stdio.h"
#include "utils/cache.h"
#include <gio/gio.h>
#include <glib.h>
#include <libintl.h>
#include <miniaudio.h>
#include <sys/ioctl.h>
#include <sys/param.h>

#define _(STRING) gettext(STRING)

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef G_USEC_PER_SEC
#define G_USEC_PER_SEC 1000000
#endif

#define SONG_MAGIC 0x534F4E47 // "SONG"

#define METADATA_MAX_LENGTH 256

typedef struct
{
        unsigned char r;
        unsigned char g;
        unsigned char b;
} PixelData;

typedef enum {
        COLOR_TYPE_RGB,
        COLOR_TYPE_ANSI
} ColorType;

typedef struct
{
        ColorType type;
        union {
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

typedef enum {
        TRACK_VIEW,
        HELP_VIEW,
        PLAYLIST_VIEW,
        LIBRARY_VIEW,
        SEARCH_VIEW
} ViewState;

typedef enum {
        COLOR_MODE_DEFAULT = 0, // Colors from ANSI 16-color palette theme
        COLOR_MODE_ALBUM = 1,   // Colors derived from album art
        COLOR_MODE_THEME = 2    // Colors from truecolor theme
} ColorMode;

typedef struct
{
        bool mouseEnabled;              // Accept mouse input or not
        int mouseLeftClickAction;       // Left mouse action
        int mouseMiddleClickAction;     // Middle mouse action
        int mouseRightClickAction;      // Right mouse action
        int mouseScrollUpAction;        // Mouse scroll up action
        int mouseScrollDownAction;      // Mouse scroll down action
        int mouseAltScrollUpAction;     // Mouse scroll up + alt action
        int mouseAltScrollDownAction;   // Mouse scroll down + alt action
        PixelData color;                // The current color, when using album derived colors
        bool coverEnabled;              // Show covers or not
        bool uiEnabled;                 // Show ui or not
        bool coverAnsi;                 // Show chafa cover (picture perfect in the right
                                        // terminal), or ascii/ansi typ cover
        bool visualizerEnabled;         // Show spectrum visualizer
        bool hideLogo;                  // No kew text at top
        bool hideHelp;                  // No help text at top
        bool hideSideCover;
        bool allowNotifications;        // Send desktop notifications or not
        int visualizer_height;          // Height in characters of the spectrum visualizer
        int visualizer_color_type;      // How colors are laid out in the spectrum
                                        // visualizer
        bool visualizerBrailleMode;     // Display the visualizer using braille
                                        // characteres
        int titleDelay;                 // Delay when drawing title in track view
        int cacheLibrary;               // Cache the library or not
        bool quitAfterStopping;         // Exit kew when the music stops or not
        bool hideGlimmeringText;        // Glimmering text on the bottom row
        time_t last_time_app_ran;       // When did this app run last, used for updating
                                        // the cached library if it has been modified
                                        // since that time
        int visualizer_bar_mode;       // 0=Thin bars, 1=Bars twice the width or 2=Auto
                                        // (Depends on window size, default)
        int replayGainCheckFirst;       // Prioritize track or album replay gain
                                        // setting
        bool saveRepeatShuffleSettings; // Save repeat and shuffle settings
                                        // between sessions. Default on.
        int repeatState;                // 0=disabled,1=repeat track ,2=repeat list
        bool shuffle_enabled;
        bool trackTitleAsWindowTitle; // Set the window title to the title of
                                      // the currently playing track
        Theme theme;                  // The color theme.
        bool themeIsSet;              // Whether a theme has been loaded;
        char theme_name[NAME_MAX];    // the name part of <theme_name>.theme, usually
                                      // lowercase first character, unlike theme.name
                                      // which is taken from within the file.
        char themeAuthor[NAME_MAX];
        ColorMode colorMode; // Which color mode to use.
        const char *VERSION;
        char *LAST_ROW;
        unsigned char default_color;
        PixelData defaultColorRGB;
        PixelData kewColorRGB;
        int chromaPreset;
        bool visualizations_instead_of_cover;
} UISettings;

typedef struct
{
        volatile bool refresh;       // Trigger a full screen refresh next update (ie
                                     // redraw cover)
        int chosen_node_id;          // The id of the tree node that is chosen in library
                                     // view
        bool allowChooseSongs;       // In library view, has the user entered a folder
                                     // that contains songs
        bool openedSubDir;           // Opening a directory in an open directory.
        int numSongsAboveSubDir;     // How many rows do we need to jump up if we
                                     // close the parent directory and open one
                                     // within
        int numDirectoryTreeEntries; // The number of entries in directory tree
                                     // in library view
        int num_progress_bars;       // The number of progress dots at the bottom of
                                     // track view
        volatile sig_atomic_t
            resizeFlag;            // Is the user resizing the terminal window
        bool resetPlaylistDisplay; // Should the playlist be reset, ie drawn
                                   // starting from playing song
        bool collapseView;         // Signal that ui needs to collapse the view
        bool isFastForwarding;
        bool isRewinding;
        bool songWasRemoved;
        bool startFromTop;
        int lastNotifiedId;
        bool noPlaylist;
        struct winsize windowSize;
        bool showLyricsPage;
        FILE *logFile;
        FileSystemEntry *currentLibEntry;
} UIState;

typedef struct
{
        Cache *tmpCache;       // Cache for temporary files
        ViewState currentView; // The current view (playlist, library, track)
                               // that kew is on
        UIState uiState;
        UISettings uiSettings;

        pthread_mutex_t data_source_mutex;
        pthread_mutex_t switch_mutex;
} AppState;

typedef struct
{
        char *key;
        char *value;
} KeyValuePair;

typedef struct
{
        char path[PATH_MAX];
        char theme[NAME_MAX];
        char ansiTheme[NAME_MAX];
        char colorMode[6];
        char coverEnabled[2];
        char coverAnsi[2];
        char useConfigColors[2];
        char visualizerEnabled[2];
        char visualizer_height[6];
        char visualizer_color_type[2];
        char titleDelay[6];
        char togglePlaylist[6];
        char toggleBindings[6];
        char volumeUp[6];
        char volumeUpAlt[6];
        char volumeDown[6];
        char previousTrackAlt[6];
        char nextTrackAlt[6];
        char scrollUpAlt[6];
        char scrollDownAlt[6];
        char switchNumberedSong[6];
        char switchNumberedSongAlt[6];
        char switchNumberedSongAlt2[6];
        char toggle_pause[6];
        char toggle_notifications[6];
        char cycleColorsDerivedFrom[6];
        char cycle_themes[6];
        char toggle_visualizer[6];
        char toggle_ascii[6];
        char toggle_repeat[6];
        char toggle_shuffle[6];
        char seekBackward[6];
        char seek_forward[6];
        char save_playlist[6];
        char add_to_favorites_playlist[6];
        char update_library[6];
        char quit[6];
        char altQuit[6];
        char hardSwitchNumberedSong[6];
        char hardPlayPause[6];
        char hardPrev[6];
        char hardNext[6];
        char hardScrollUp[6];
        char hardScrollDown[6];
        char hardShowPlaylist[6];
        char hardShowPlaylistAlt[6];
        char showPlaylistAlt[6];
        char hardShowKeys[6];
        char hardShowKeysAlt[6];
        char showKeysAlt[6];
        char hardEndOfPlaylist[6];
        char hardShowLibrary[6];
        char hardShowLibraryAlt[6];
        char showLibraryAlt[6];
        char hardShowSearch[6];
        char hardShowSearchAlt[6];
        char showSearchAlt[6];
        char hardShowTrack[6];
        char hardShowTrackAlt[6];
        char showTrackAlt[6];
        char nextPage[6];
        char prevPage[6];
        char hardRemove[6];
        char hardRemove2[6];
        char mouseLeftClick[12];
        char mouseMiddleClick[12];
        char mouseRightClick[12];
        char mouseScrollUp[12];
        char mouseScrollDown[12];
        char mouseAltScrollUp[12];
        char mouseAltScrollDown[12];
        char lastVolume[12];
        char allowNotifications[2];
        char color[2];
        char artistColor[2];
        char enqueued_color[2];
        char titleColor[2];
        char mouseEnabled[2];
        char mouseLeftClickAction[3];
        char mouseMiddleClickAction[3];
        char mouseRightClickAction[3];
        char mouseScrollUpAction[3];
        char mouseScrollDownAction[3];
        char mouseAltScrollUpAction[3];
        char mouseAltScrollDownAction[3];
        char hideLogo[2];
        char hideHelp[2];
        char hideSideCover[2];
        char quitAfterStopping[2];
        char hideGlimmeringText[2];
        char nextView[6];
        char prevView[6];
        char hardClearPlaylist[6];
        char move_song_up[6];
        char move_song_down[6];
        char enqueueAndPlay[6];
        char hardStop[6];
        char sort_library[6];
        char visualizerBrailleMode[2];
        char progressBarElapsedEvenChar[12];
        char progressBarElapsedOddChar[12];
        char progressBarApproachingEvenChar[12];
        char progressBarApproachingOddChar[12];
        char progressBarCurrentEvenChar[12];
        char progressBarCurrentOddChar[12];
        char visualizer_bar_width[2];
        char replayGainCheckFirst[2];
        char saveRepeatShuffleSettings[2];
        char repeatState[2];
        char shuffle_enabled[2];
        char trackTitleAsWindowTitle[2];
        char showLyricsPage[6];
        char chromaPreset[6];
} AppSettings;

typedef struct
{
        int row;
        int col;
        int length;
} ProgressBar;

typedef struct
{
        char title[METADATA_MAX_LENGTH];
        char artist[METADATA_MAX_LENGTH];
        char album_artist[METADATA_MAX_LENGTH];
        char album[METADATA_MAX_LENGTH];
        char date[METADATA_MAX_LENGTH];
        double replaygainTrack;
        double replaygainAlbum;
} TagSettings;

typedef struct
{
        int magic;
        gchar *track_id;
        char file_path[PATH_MAX];
        char cover_art_path[PATH_MAX];
        unsigned char red;
        unsigned char green;
        unsigned char blue;
        TagSettings *metadata;
        unsigned char *cover;
        int avg_bit_rate;
        int coverWidth;
        int coverHeight;
        double duration;
        bool hasErrors;
        Lyrics *lyrics;
} SongData;

typedef struct
{
        char file_path[PATH_MAX];
        SongData *songdataA;
        SongData *songdataB;
        bool loadA;
        bool loadingFirstDecoder;
        pthread_mutex_t mutex;
        AppState *state;
} LoadingThreadData;

typedef struct
{
        LoadingThreadData loadingdata;
        int lastPlayedId;
        bool skipping;
        bool songLoading;
        bool forceSkip;
        bool nextSongNeedsRebuilding;
        bool skipOutOfOrder;
        bool hasSilentlySwitched;
        bool usingSongDataA;
        bool clearingErrors;
        bool songHasErrors;
        bool skipFromStopped;
        bool waitingForNext;     // Playlist has songs but playback is stopped.
        bool waitingForPlaylist; // Playlist is empty.
        bool notifySwitch;       // Emit mpris song switched signal
        bool notifyPlaying;      // Emit mpris music is playing signal
        volatile bool loadedNextSong;
} PlaybackState;

typedef struct
{
        SongData *songdataA;
        SongData *songdataB;
        bool songdataADeleted;
        bool songdataBDeleted;
        int replayGainCheckFirst;
        SongData *current_song_data;
        ma_uint64 currentPCMFrame;
} UserData;

typedef struct
{
        ma_data_source_base base;
        UserData *pUserData;
        ma_format format;
        ma_uint32 channels;
        ma_uint32 sample_rate;
        ma_uint64 currentPCMFrame;
        ma_uint32 avg_bit_rate;
        bool switchFiles;
        int currentFileIndex;
        ma_uint64 totalFrames;
        bool end_of_list_reached;
        bool restart;
} AudioData;

extern AudioData audio_data;

// --- Getters ---

PlaybackState *get_playback_state(void);
AudioData *get_audio_data(void);
double get_pause_seconds(void);
double get_total_pause_seconds(void);
void create_playlist(PlayList **playlist);
Node *get_next_song(void);
Node *get_song_to_start_from(void);
Node *get_try_next_song(void);
PlayList *get_playlist(void);
PlayList *get_unshuffled_playlist(void);
PlayList *get_favorites_playlist(void);
ProgressBar *get_progress_bar(void);
AppState *get_app_state(void);
AppSettings *get_app_settings(void);
FileSystemEntry *get_library(void);
char *get_library_file_path(void);

// --- Setters ---

void set_pause_seconds(double seconds);
void set_total_pause_seconds(double seconds);
void set_next_song(Node *node);
void set_song_to_start_from(Node *node);
void set_try_next_song(Node *node);
void set_audio_data(AudioData *audio_data);
void set_library(FileSystemEntry *root);
void free_playlists(void);
void set_unshuffled_playlist(PlayList *pl);
void set_favorites_playlist(PlayList *pl);

#endif
