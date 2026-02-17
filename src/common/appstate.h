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

/**
 * @brief Represents an RGB pixel color.
 *
 * Stores red, green, and blue components as 8-bit unsigned values.
 */
typedef struct
{
        unsigned char r; /**< Red component (0–255). */
        unsigned char g; /**< Green component (0–255). */
        unsigned char b; /**< Blue component (0–255). */
} PixelData;

/**
 * @brief Defines the type of color representation.
 */
typedef enum {
        COLOR_TYPE_RGB,  /**< 24-bit RGB color. */
        COLOR_TYPE_ANSI  /**< 16-color ANSI palette index. */
} ColorType;

/**
 * @brief Represents a color value in either RGB or ANSI format.
 */
typedef struct
{
        ColorType type; /**< The underlying color representation type. */
        union {
                PixelData rgb;   /**< RGB color value. */
                int8_t ansiIndex; /**< ANSI index (-1 to 15). -1 = default foreground. */
        };
} ColorValue;

/**
 * @brief Defines a complete UI theme.
 *
 * Contains all configurable color roles used throughout the UI.
 */
typedef struct
{
        char theme_name[NAME_MAX];     /**< Theme display name. */
        char theme_author[NAME_MAX];   /**< Theme author name. */

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

/**
 * @brief Represents the active UI view.
 */
typedef enum {
        TRACK_VIEW,    /**< Track detail view. */
        HELP_VIEW,     /**< Help screen. */
        PLAYLIST_VIEW, /**< Playlist view. */
        LIBRARY_VIEW,  /**< Library browser view. */
        SEARCH_VIEW    /**< Search results view. */
} ViewState;

/**
 * @brief Defines how UI colors are selected.
 */
typedef enum {
        COLOR_MODE_DEFAULT = 0, /**< ANSI 16-color palette theme. */
        COLOR_MODE_ALBUM = 1,   /**< Colors derived from album art. */
        COLOR_MODE_THEME = 2    /**< Truecolor theme file. */
} ColorMode;

/**
 * @brief Stores user-configurable UI settings.
 *
 * Contains visual preferences, behavior toggles, theme information,
 * and runtime UI options.
 */
typedef struct
{
        bool mouseEnabled;            /**< Accept mouse input or not. */
        int mouseLeftClickAction;     /**< Action triggered by left mouse click. */
        int mouseMiddleClickAction;   /**< Action triggered by middle mouse click. */
        int mouseRightClickAction;    /**< Action triggered by right mouse click. */
        int mouseScrollUpAction;      /**< Action triggered by mouse scroll up. */
        int mouseScrollDownAction;    /**< Action triggered by mouse scroll down. */
        int mouseAltScrollUpAction;   /**< Action triggered by Alt + scroll up. */
        int mouseAltScrollDownAction; /**< Action triggered by Alt + scroll down. */

        PixelData color; /**< Album-derived accent color. */

        bool coverEnabled;       /**< Whether album covers are displayed. */
        bool uiEnabled;          /**< Whether the user interface is rendered. */
        bool coverAnsi;          /**< Use high-quality chafa cover rendering if supported,
                                      otherwise ASCII/ANSI cover. */
        bool visualizerEnabled;  /**< Show spectrum visualizer. */
        bool discordRPCEnabled;  /**< Enable Discord Rich Presence integration. */
        bool hideLogo;           /**< Hide application logo at the top. */
        bool hideHelp;           /**< Hide help text at the top. */
        bool hideFooter;         /**< Hide footer section. */
        bool hideSideCover;      /**< Hide side cover panel. */
        bool allowNotifications; /**< Enable desktop notifications. */

        int visualizer_height;      /**< Height (in terminal rows) of the spectrum visualizer. */
        int visualizer_color_type;  /**< Color layout mode for the spectrum visualizer. */
        bool visualizerBrailleMode; /**< Render visualizer using braille characters. */

        int titleDelay;           /**< Delay before drawing title in track view (ms). */
        int cacheLibrary;         /**< Whether to cache the music library. */
        bool quitAfterStopping;   /**< Exit application automatically after playback stops. */
        bool hideGlimmeringText;  /**< Disable animated/glimmering bottom row text. */
        time_t last_time_app_ran; /**< Timestamp of last run, used to detect library changes. */

        int visualizer_bar_mode;        /**< 0=Thin bars, 1=Double width bars, 2=Auto (default). */
        int replayGainCheckFirst;       /**< Priority of replay gain mode (track vs album). */
        bool saveRepeatShuffleSettings; /**< Persist repeat/shuffle settings between sessions. */
        int repeatState;                /**< 0=Disabled, 1=Repeat track, 2=Repeat list. */
        bool shuffle_enabled;           /**< Whether shuffle mode is enabled. */
        bool trackTitleAsWindowTitle;   /**< Set terminal window title to current track title. */

        Theme theme;          /**< Active theme. */
        bool themeIsSet;      /**< Whether a theme has been loaded. */
        char theme_name[NAME_MAX]; /**< Theme filename (without extension). */
        char themeAuthor[NAME_MAX]; /**< Author name stored from theme file. */

        ColorMode colorMode;  /**< Current color mode. */

        const char *VERSION; /**< Application version string. */
        char *LAST_ROW;      /**< Cached content of the terminal's last row. */

        unsigned char default_color; /**< Default ANSI color index. */
        PixelData defaultColorRGB;   /**< Default RGB color value. */
        PixelData kewColorRGB;       /**< Primary application accent RGB color. */

        int chromaPreset;                  /**< Preset index for chroma-based coloring. */
        bool stripTrackNumbers;            /**< Remove track numbers from displayed titles. */
        bool visualizations_instead_of_cover; /**< Show visualizer instead of album cover. */
} UISettings;

/**
 * @brief Stores dynamic UI runtime state.
 */
typedef struct
{
        volatile bool refresh;
        int chosen_node_id;
        bool allowChooseSongs;
        bool allowChooseSearchSongs;

        bool openedSubDir;
        int numSongsAboveSubDir;
        bool openedSearchSubDir;
        int numSongsAboveSearchSubDir;

        int numDirectoryTreeEntries;
        int num_progress_bars;

        volatile sig_atomic_t resizeFlag;

        bool resetPlaylistDisplay;
        bool collapseView;
        bool collapseSearchView;

        int previous_chosen_search_row;
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
        FileSystemEntry *currentSearchEntry;
} UIState;

/**
 * @brief Global application state.
 *
 * Holds UI state, settings, caches, and synchronization primitives.
 */
typedef struct
{
        Cache *tmpCache;
        ViewState currentView;
        UIState uiState;
        UISettings uiSettings;

        pthread_mutex_t data_source_mutex;
        pthread_mutex_t switch_mutex;
} AppState;

/**
 * @brief Represents a key-value pair from configuration.
 */
typedef struct
{
        char *key;
        char *value;
} KeyValuePair;

/**
 * @brief Represents persisted application settings loaded from config.
 */
typedef struct
{
        char path[PATH_MAX];
        /* (All remaining fields are serialized config string values) */
} AppSettings;

/**
 * @brief Describes a progress bar position and size.
 */
typedef struct
{
        int row;    /**< Terminal row. */
        int col;    /**< Terminal column. */
        int length; /**< Length in characters. */
} ProgressBar;

/**
 * @brief Stores metadata tags extracted from audio files.
 */
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

/**
 * @brief Holds decoded audio track data and metadata.
 */
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

/**
 * @brief Data passed to the loading thread for asynchronous decoding.
 */
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

/**
 * @brief Runtime playback state.
 */
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

        bool waitingForNext;
        bool waitingForPlaylist;

        bool notifySwitch;
        bool notifyPlaying;

        volatile bool loadedNextSong;
} PlaybackState;

/**
 * @brief Per-audio-device user data for decoding and playback.
 */
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

/**
 * @brief Audio device state and streaming data.
 */
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

/**
 * @brief Global audio data instance.
 */
extern AudioData audio_data;

/* ========================= GETTERS ========================= */

/** @brief Get the global playback state. */
PlaybackState *get_playback_state(void);

/** @brief Get the global audio data structure. */
AudioData *get_audio_data(void);

/** @brief Get current pause duration in seconds. */
double get_pause_seconds(void);

/** @brief Get total accumulated pause duration. */
double get_total_pause_seconds(void);

/** @brief Create a new playlist instance. */
void create_playlist(PlayList **playlist);

/** @brief Get the next song in playback order. */
Node *get_next_song(void);

/** @brief Get the song designated to start playback from. */
Node *get_song_to_start_from(void);

/** @brief Get the candidate next song during rebuilding. */
Node *get_try_next_song(void);

/** @brief Get the active shuffled playlist. */
PlayList *get_playlist(void);

/** @brief Get the unshuffled playlist. */
PlayList *get_unshuffled_playlist(void);

/** @brief Get the favorites playlist. */
PlayList *get_favorites_playlist(void);

/** @brief Get the progress bar descriptor. */
ProgressBar *get_progress_bar(void);

/** @brief Get the global application state. */
AppState *get_app_state(void);

/** @brief Get application configuration settings. */
AppSettings *get_app_settings(void);

/** @brief Get the root library entry. */
FileSystemEntry *get_library(void);

/** @brief Get the library root file path. */
char *get_library_file_path(void);

/* ========================= SETTERS ========================= */

/** @brief Set pause duration in seconds. */
void set_pause_seconds(double seconds);

/** @brief Set total accumulated pause duration. */
void set_total_pause_seconds(double seconds);

/** @brief Set the next song pointer. */
void set_next_song(Node *node);

/** @brief Set the song to start playback from. */
void set_song_to_start_from(Node *node);

/** @brief Set a temporary next-song candidate. */
void set_try_next_song(Node *node);

/** @brief Replace the global audio data pointer. */
void set_audio_data(AudioData *audio_data);

/** @brief Set the library root entry. */
void set_library(FileSystemEntry *root);

/** @brief Free all playlist instances. */
void free_playlists(void);

/** @brief Set the unshuffled playlist instance. */
void set_unshuffled_playlist(PlayList *pl);

/** @brief Set the favorites playlist instance. */
void set_favorites_playlist(PlayList *pl);


#endif
