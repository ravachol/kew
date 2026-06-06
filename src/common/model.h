#ifndef MODEL_H
#define MODEL_H

#include "data/directorytree.h"
#include "loader/songdatatype.h"

#include "data/playlist.h"

#include "sound/sound_facade.h"

#include "utils/img_utils.h"

#include "common/path_max.h"
#include "common/events.h"

#include "stdio.h"
#include <gio/gio.h>
#include <glib.h>
#include <libintl.h>
#include <miniaudio.h>
#include <stdatomic.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/param.h>

extern sound_system_t *sound_sys;

#define ATTR_NONE 0
#define ATTR_BOLD (1 << 0)
#define ATTR_DIM (1 << 1)
#define ATTR_ITALIC (1 << 2)
#define ATTR_UNDERLINE (1 << 3)
#define ATTR_BLINK (1 << 4)
#define ATTR_REVERSE (1 << 5)

#define MAX_PANES 100
#define MAX_ROWS 100

#define MAX_SEARCH_LEN 32

typedef enum {
        DIRTY_NONE = 0,
        DIRTY_VISUALIZER = 1 << 0, // redraws 60fps
        DIRTY_PROGRESS = 1 << 1,   // redraws ~4fps
        DIRTY_SONG = 1 << 2,
        DIRTY_TITLE = 1 << 3,
        DIRTY_PLAYLIST = 1 << 4,
        DIRTY_LIBRARY = 1 << 5,
        DIRTY_SEARCH = 1 << 6,
        DIRTY_FOOTER = 1 << 7,
        DIRTY_LAYOUT = 1 << 8,
        DIRTY_CHROMA = 1 << 9,

        // redraw on full-screen refresh
        DIRTY_REFRESH    = 1 << 9,

        DIRTY_ALL = ~0
} DirtyFlags;

typedef enum {
        CELL_NORMAL,        // regular character cell
        CELL_WIDE_CONT,     // right half of a double-width character, backend skips it
        CELL_IMAGE_ANCHOR,  // top-left cell of an image region, backend emits the image here
        CELL_OCCUPIED,      // remaining cells of an occupied region, backend skips them
        CELL_LINK
} CellKind;

typedef enum {
        FADE_QUICK,
        FADE_MEDIUM,
        FADE_SLOW
} FadeKind;

typedef struct {
        int row;
        int col;
        int width;
        int height;
} k_Rect;

typedef struct {
        PixelData fg;
        PixelData bg;
        int fgAnsi;
        int bgAnsi;
        bool isAnsi;
        uint8_t attrs; // ATTR_BOLD, ATTR_DIM, ATTR_ITALIC, etc.
} CellStyle;

typedef enum {
        ALIGN_LEFT,
        ALIGN_CENTER,
        ALIGN_RIGHT,
} Align;

typedef enum {
        SIZE_AUTO,    // fill remaining space
        SIZE_FIXED,   // fixed number of rows/cols
        SIZE_PERCENT, // percentage of parent
        SIZE_WINDOW_MINUS,
        SIZE_INDENT,
        SIZE_INDENT_WIDE,
        SIZE_FROM_HEIGHT,  // width = f(height)
        SIZE_FROM_WIDTH,   // height = f(width)
} SizeKind;

typedef enum {
        COL_INDENT = -1,
        COL_INDENT_WIDE = -2,
} ColKind;

typedef struct {
        SizeKind kind;
        int value; // lines if FIXED, 0-100 if PERCENT, ignored if AUTO
} k_Size;

typedef enum {
    IMAGE_SIXEL,
    IMAGE_KITTY,
    IMAGE_ITERM2,
    IMAGE_PIXEL_DIRECT,
} ImageProtocol;

typedef struct {
    ImageProtocol protocol;
    uint8_t      *data;       // encoded sixel/kitty blob, or raw RGBA
    size_t        data_len;
    int           pixel_w;
    int           pixel_h;
    uint64_t      id;         // for change detection / kitty image IDs
    int screen_w;
    int screen_h;
} ImagePayload;

typedef struct {
    char         *url;
    char         *title;
} LinkPayload;

typedef struct {
        k_Size width;  // AUTO = fill remaining cols in row

        uint32_t codepoint;
        uint8_t attrs;
        CellKind kind;
        CellStyle style;
        ImagePayload *image; // non-null only on CELL_IMAGE_ANCHOR
        LinkPayload *link;
} Cell;

typedef struct {
        Cell *cells; // [rows * cols]
        int cols, rows;
        bool *dirty_rows; // bitmask, for fast skip
} DrawBuffer;

typedef struct Model Model;

struct Msg {
        enum MsgType type;
        char key[MAX_SEQ_LEN]; // To store multi-byte characters
        char args[32];

        int chosen_lib_row;
        FileSystemEntry *current_lib_entry;
        int num_lib_rows;

        int chosen_search_result_row;
        FileSystemEntry *current_search_entry;

        k_Rect region;

        int lyrics_offset;
};

typedef struct {
    bool has_msg;
    struct Msg msg;
} ComponentMsg;

typedef ComponentMsg (*ComponentFn)(
    const Model *model,
    k_Rect region,
    DrawBuffer *buf,
    DirtyFlags dirty);

typedef struct Layout Layout;

typedef struct {
    ComponentFn fn;
    DirtyFlags  redraws_on;
    Align       align;
    k_Size        width;
    k_Rect      region;        // resolved by layout_reflow()
    int         offsetX;
    int         offsetY;
    int         hidden;
    Layout *child;
} Pane;

typedef struct {
    Pane    panes[MAX_PANES];
    int     pane_count;
    k_Size    height;
    int     col;
    int     resolved_height;   // set by layout_reflow()
    bool    hidden;
} Row;

typedef struct Layout {
    Row     rows[MAX_ROWS];
    int     row_count;
} Layout;

/**
 * @brief Defines the type of color representation.
 */
typedef enum {
        COLOR_TYPE_RGB, /**< 24-bit RGB color. */
        COLOR_TYPE_ANSI /**< 16-color ANSI palette index. */
} ColorType;

/**
 * @brief Represents a color value in either RGB or ANSI format.
 */
typedef struct
{
        ColorType type; /**< The underlying color representation type. */
        union {
                PixelData rgb;    /**< RGB color value. */
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
        char theme_name[NAME_MAX];   /**< Theme display name. */
        char theme_author[NAME_MAX]; /**< Theme author name. */

        ColorValue accent;
        ColorValue text;
        ColorValue textDim;
        ColorValue textMuted;
        ColorValue logo;
        ColorValue header;
        ColorValue footer;
        ColorValue help;
        ColorValue link;
        ColorValue playbackstatus;
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
        COLOR_MODE_DEFAULT = 0,         /**< ANSI 16-color palette theme. */
        COLOR_MODE_ALBUM_ONE = 1,       /**< Colors derived from album art using only one color. */
        COLOR_MODE_THEME = 2,           /**< Truecolor theme file. */
        COLOR_MODE_ALBUM = 3,           /**< Colors derived from album art. */
        COLOR_MODE_NEUTRAL = 4            /**< No color. */
} ColorMode;

typedef enum {
        VIZ_LIGHTEN = 0,
        VIZ_FLAT = 1,
        VIZ_REVERSED = 2,
        VIZ_KMEANS_CLUSTERING = 3,
        VIZ_VIBRANT = 4,
        VIZ_LUM_VIBRANT = 5,
        VIZ_BINNING = 6,
        VIZ_OFF = 7,
} VisualizerMode;

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
        char coverStyle[16];     /**< Chafa symbol style: auto, kitty, sixels, block, braille, ascii, dot, vhalf, quad. */
        VisualizerMode visualizer_mode;     /**< Visualizer mode selector. */
        bool discordRPCEnabled;  /**< Enable Discord Rich Presence integration. */
        bool hideLogo;           /**< Hide application logo at the top. */
        bool hideHelp;           /**< Hide help text at the top. */
        bool hideFooter;         /**< Hide footer section. */
        bool hideTimeStatus;     /**< Hide elapsed, song length, vol, bitrate. */
        bool simpleTimeStatus;     /**< Shows only elapsed seconds and total seconds. */
        bool hideSideCover;      /**< Hide side cover panel. */
        bool collapseTopLevel;      /**< Hide contents of top level folders. */
        bool allowNotifications; /**< Enable desktop notifications. */

        int visualizer_height;      /**< Height (in terminal rows) of the spectrum visualizer. */
        bool visualizerBrailleMode; /**< Render visualizer using braille characters. */

        int titleDelay;           /**< Delay before drawing title in track view (ms). */
        int cacheLibrary;         /**< Whether to cache the music library. */
        bool quitAfterStopping;   /**< Exit application automatically after playback stops. */
        bool clearListClearsAll;  /**< Whether clearing the playlist also removes the currently playing song. */
        bool hideGlimmeringText;  /**< Disable animated/glimmering bottom row text. */
        time_t last_time_app_ran; /**< Timestamp of last run, used to detect library changes. */

        int visualizer_bar_mode;        /**< 0=Thin bars, 1=Double width bars, 2=Auto (default). */
        int replayGainCheckFirst;       /**< Priority of replay gain mode (track vs album). */
        bool saveRepeatShuffleSettings; /**< Persist repeat/shuffle settings between sessions. */
        int repeatState;                /**< 0=Disabled, 1=Repeat track, 2=Repeat list. */
        bool shuffle_enabled;           /**< Whether shuffle mode is enabled. */
        bool trackTitleAsWindowTitle;   /**< Set terminal window title to current track title. */

        Theme theme;                /**< Active theme. */
        bool themeIsSet;            /**< Whether a theme has been loaded. */
        char theme_name[NAME_MAX];  /**< Theme filename (without extension). */
        char themeAuthor[NAME_MAX]; /**< Author name stored from theme file. */

        int lastVolume; /* volume the last time kew was run, int between 0 and 100. */

        ColorMode colorMode; /**< Current color mode. */

        const char *VERSION; /**< Application version string. */
        char *LAST_ROW;      /**< Cached content of the terminal's last row. */

        unsigned char default_color; /**< Default ANSI color index. */
        PixelData defaultColorRGB;   /**< Default RGB color value. */
        PixelData kewColorRGB;       /**< Primary application accent RGB color. */

        int chromaPreset; /**< Preset index for chroma-based coloring. */
        char chromaPath[PATH_MAX];
        char chromaDevice[PATH_MAX];
        bool stripTrackNumbers;               /**< Remove track numbers from displayed titles. */
        bool visualizations_instead_of_cover; /**< Show visualizer instead of album cover. */

        PixelData footer_color;

        bool showFoldersInPlaylist;           /**< Group playlist tracks by folder. */

        double currentSongSeconds;
        int currentSongId;
        int auto_resume;

        int always_crossfade;
        int fade_enter_song_ms;
        int fade_quick_ms;
        int fade_medium_ms;
        int fade_slow_ms;
} UISettings;

typedef struct {
        int start_lib_iter;
        FileSystemEntry *chosen_dir;
        int previous_chosen_row;
} TreeContext;

typedef struct {
        PixelData colors[16];
        int count;
} ColorPalette;

/**
 * @brief Stores dynamic UI runtime state.
 */
typedef struct
{
        volatile bool refresh;
        int chosen_node_id;
        bool allowChooseSongs;
        bool allowChooseSearchSongs;

        int numDirectoryTreeEntries;
        int num_progress_bars;

        volatile sig_atomic_t resizeFlag;

        bool resetPlaylistDisplay;

        bool isFastForwarding;
        bool isRewinding;
        bool songWasRemoved;
        bool startFromTop;
        int lastNotifiedId;
        bool noPlaylist;

        struct winsize windowSize;

        bool showLyricsPage;

        FILE *logFile;

        char search_text[MAX_SEARCH_LEN * 4 + 1]; // Unicode can be 4 characters

        FileSystemEntry *current_lib_entry;
        FileSystemEntry *current_search_entry;
        FileSystemEntry *chosen_search_dir;
        FileSystemEntry *chosen_dir;
        FileSystemEntry *last_search_parent;

        TreeContext treeCtx;

        Node *playlist_node;
        int current_at_row;

        int search_results_count;

        float aspect_ratio;

        int chosen_row;
        int chosen_lib_row;
        int chosen_search_result_row;

        int start_lib_iter;
        int start_iter;
        int previous_chosen_song;

        int footer_row;
        int footer_col;

        int visualizer_width;

        const char *lyrics_line;

        bool render_often;
        bool render_search;

        int max_playlist_rows;
        int max_lib_rows;
        int max_search_rows;
        int lib_row_count;
        bool check_collapse_top_level;

        int chosen_lyrics_row;

        int has_chroma;
        bool chroma_started;
        bool chroma_next_preset_requested;
        bool chroma_start_requested;
        int chroma_height;

        bool link_clicked;

        ColorPalette visualizer_palettes[7];
} UIState;

/**
 * @brief Global application state.
 *
 * Holds UI state, settings, caches, and synchronization primitives.
 */
typedef struct
{
        ViewState currentView;
        ViewState last_view;
        UIState ui;
        UISettings settings;

        pthread_mutex_t library_mutex;
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
        char theme[NAME_MAX];
        char ansiTheme[NAME_MAX];
        char colorMode[6];
        char coverEnabled[2];
        char coverAnsi[2];
        char coverStyle[16];
        char useConfigColors[2];
        char visualizer_mode[2];
        char visualizer_color_type[2];
        char discordRPCEnabled[2];
        char visualizer_height[6];
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
        char toggle_crossfade[6];
        char cycleColorsDerivedFrom[6];
        char cycle_themes[6];
        char toggle_visualizer[6];
        char toggle_ascii[6];
        char toggle_repeat[6];
        char toggle_shuffle[6];
        char auto_resume[6];
        char always_crossfade[6];
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
        char hideFooter[2];
        char hideSideCover[2];
        char collapseTopLevel[2];
        char hideTimeStatus[2];
        char simpleTimeStatus[2];
        char quitAfterStopping[2];
        char clearListClearsAll[2];
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
        char currentSongId[12];
        char currentSongSeconds[12];
        char visualizer_bar_width[2];
        char replayGainCheckFirst[2];
        char saveRepeatShuffleSettings[2];
        char repeatState[2];
        char shuffle_enabled[2];
        char trackTitleAsWindowTitle[2];
        char showLyricsPage[6];
        char stripTrackNumbers[2];
        char chromaPreset[6];
        char chromaPath[PATH_MAX];
        char chromaDevice[PATH_MAX];
        char showFoldersInPlaylist[2];
        char fade_enter_song_ms[12];
        char fade_quick[6];
        char fade_medium[6];
        char fade_slow[6];
        char fade_quick_ms[12];
        char fade_medium_ms[12];
        char fade_slow_ms[12];
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
 * @brief Runtime playback state.
 */
typedef struct
{
        int lastPlayedId;

        bool skipping;
        bool songLoading;
        bool forceSkip;
        bool nextSongNeedsRebuilding;
        bool skipOutOfOrder;
        bool hasSilentlySwitched;
        bool clearingErrors;
        bool songHasErrors;
        bool skipFromStopped;

        bool waitingForNext;
        bool waitingForPlaylist;

        bool notifySwitch;
        bool notifyPlaying;
        bool notifySeek;

        volatile bool loadedNextSong;

        pthread_mutex_t switch_mutex;
} PlaybackState;

typedef struct {
        bool active;
        int frame;
        int num_frames;
        int pre_anim_delay_frame;
        PixelData color;
} AnimationState;

typedef struct
{
        gint width_cells, height_cells;
        gint width_pixels, height_pixels;
} TermSize;

typedef struct SearchResult {
        FileSystemEntry *entry;
        struct FileSystemEntry *parent;
        int distance;
        int groupDistance;
        int num_children;
} SearchResult;

typedef struct Model {
        AppState state;
        AppSettings settings;
        PlaybackState playbackState;
        ProgressBar progressBar;

        PlayList *playlist;
        PlayList *unshuffled_playlist;
        PlayList *favorites_playlist;

        FileSystemEntry *library;
        volatile bool library_updated;

        SongData *songdata;

        double elapsed_seconds;

        int volume;

        bool is_paused;
        bool is_stopped;
        bool should_refresh;
        bool last_paused_state;
        bool restore_volume;

        bool songdata_ok;

        int term_w;
        int term_h;
        TermSize term_size; // FIXME only one of (term_w, term_h) and term_size are needed

        int preferred_width;
        int preferred_height;
        int min_height;

        int indent;
        int indent_wide;

        int updateCounter;

        const char *current_path;

        // Compute hash of current album cover path
        size_t current_hash;
        size_t last_cover_path_hash;

        volatile sig_atomic_t dirty;

        AnimationState glimmer;

        AnimationState title_delay;

        AnimationState name_scroll;

        SearchResult *search_results;

        int tick;

        Layout *playlist_layout;
        Layout *library_layout;
        Layout *track_layout;
        Layout *track_landscape_layout;
        Layout *search_layout;
        Layout *help_layout;
} Model;

#endif
