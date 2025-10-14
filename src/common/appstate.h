/**
 * @file appstate.h
 * @brief Provides globally accessible state structs, getters and setters.
 *
 */

#ifndef APPSTATE_H
#define APPSTATE_H

#include "utils/cache.h"
#include "data/lyrics.h"
#include "data/playlist.h"
#include "stdio.h"
#include "data/theme.h"
#include <gio/gio.h>
#include <glib.h>
#include <miniaudio.h>
#include <sys/ioctl.h>
#include <sys/param.h>

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

#ifndef G_USEC_PER_SEC
#define G_USEC_PER_SEC 1000000
#endif

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
        TRACK_VIEW,
        KEYBINDINGS_VIEW,
        PLAYLIST_VIEW,
        LIBRARY_VIEW,
        SEARCH_VIEW
} ViewState;

typedef enum
{
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
        bool allowNotifications;        // Send desktop notifications or not
        int visualizerHeight;           // Height in characters of the spectrum visualizer
        int visualizerColorType;        // How colors are laid out in the spectrum
                                        // visualizer
        bool visualizerBrailleMode;     // Display the visualizer using braille
                                        // characteres
        int titleDelay;                 // Delay when drawing title in track view
        int cacheLibrary;               // Cache the library or not
        bool quitAfterStopping;         // Exit kew when the music stops or not
        bool hideGlimmeringText;        // Glimmering text on the bottom row
        time_t lastTimeAppRan;          // When did this app run last, used for updating
                                        // the cached library if it has been modified
                                        // since that time
        int visualizerBarWidth;         // 0=Thin bars, 1=Bars twice the width or 2=Auto
                                        // (Depends on window size, default)
        int replayGainCheckFirst;       // Prioritize track or album replay gain
                                        // setting
        bool saveRepeatShuffleSettings; // Save repeat and shuffle settings
                                        // between sessions. Default on.
        int repeatState;                // 0=disabled,1=repeat track ,2=repeat list
        bool shuffleEnabled;
        bool trackTitleAsWindowTitle; // Set the window title to the title of
                                      // the currently playing track
        Theme theme;                  // The color theme.
        bool themeIsSet;              // Whether a theme has been loaded;
        char themeName[NAME_MAX];     // the name part of <themeName>.theme, usually
                                      // lowercase first character, unlike theme.name
                                      // which is taken from within the file.
        char themeAuthor[NAME_MAX];
        ColorMode colorMode; // Which color mode to use.
        const char *VERSION;
        char *LAST_ROW;
        unsigned char defaultColor;
        PixelData defaultColorRGB;
        PixelData kewColorRGB;
} UISettings;

typedef struct
{
        volatile bool refresh;       // Trigger a full screen refresh next update (ie
                                     // redraw cover)
        int chosenNodeId;            // The id of the tree node that is chosen in library
                                     // view
        bool allowChooseSongs;       // In library view, has the user entered a folder
                                     // that contains songs
        bool openedSubDir;           // Opening a directory in an open directory.
        int numSongsAboveSubDir;     // How many rows do we need to jump up if we
                                     // close the parent directory and open one
                                     // within
        int numDirectoryTreeEntries; // The number of entries in directory tree
                                     // in library view
        int numProgressBars;         // The number of progress dots at the bottom of
                                     // track view
        volatile sig_atomic_t
            resizeFlag;             // Is the user resizing the terminal window
        bool resetPlaylistDisplay;  // Should the playlist be reset, ie drawn
                                    // starting from playing song
        bool collapseView;          // Signal that ui needs to collapse the view
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

        pthread_mutex_t dataSourceMutex;
        pthread_mutex_t switchMutex;
} AppState;

#ifndef KEYVALUEPAIR_STRUCT
#define KEYVALUEPAIR_STRUCT

typedef struct
{
        char *key;
        char *value;
} KeyValuePair;

#endif

#ifndef APPSETTINGS_STRUCT

typedef struct
{
        char path[MAXPATHLEN];
        char theme[NAME_MAX];
        char ansiTheme[NAME_MAX];
        char colorMode[6];
        char coverEnabled[2];
        char coverAnsi[2];
        char useConfigColors[2];
        char visualizerEnabled[2];
        char visualizerHeight[6];
        char visualizerColorType[2];
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
        char togglePause[6];
        char toggleNotifications[6];
        char cycleColorsDerivedFrom[6];
        char cycleThemes[6];
        char toggleVisualizer[6];
        char toggleAscii[6];
        char toggleRepeat[6];
        char toggleShuffle[6];
        char seekBackward[6];
        char seekForward[6];
        char savePlaylist[6];
        char addToFavoritesPlaylist[6];
        char updateLibrary[6];
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
        char enqueuedColor[2];
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
        char cacheLibrary[6];
        char quitAfterStopping[2];
        char hideGlimmeringText[2];
        char nextView[6];
        char prevView[6];
        char hardClearPlaylist[6];
        char moveSongUp[6];
        char moveSongDown[6];
        char enqueueAndPlay[6];
        char hardStop[6];
        char sortLibrary[6];
        char visualizerBrailleMode[2];
        char progressBarElapsedEvenChar[12];
        char progressBarElapsedOddChar[12];
        char progressBarApproachingEvenChar[12];
        char progressBarApproachingOddChar[12];
        char progressBarCurrentEvenChar[12];
        char progressBarCurrentOddChar[12];
        char visualizerBarWidth[2];
        char replayGainCheckFirst[2];
        char saveRepeatShuffleSettings[2];
        char repeatState[2];
        char shuffleEnabled[2];
        char trackTitleAsWindowTitle[2];
        char showLyricsPage[6];
} AppSettings;

#endif

#ifndef PROGRESSBAR_STRUCT
#define PROGRESSBAR_STRUCT

typedef struct
{
        int row;
        int col;
        int length;
} ProgressBar;

#endif

#ifndef TAGSETTINGS_STRUCT
#define TAGSETTINGS_STRUCT

#define METADATA_MAX_LENGTH 256

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

#endif

#ifndef SONGDATA_STRUCT
#define SONGDATA_STRUCT
typedef struct
{
        gchar *trackId;
        char filePath[MAXPATHLEN];
        char coverArtPath[MAXPATHLEN];
        unsigned char red;
        unsigned char green;
        unsigned char blue;
        TagSettings *metadata;
        unsigned char *cover;
        int avgBitRate;
        int coverWidth;
        int coverHeight;
        double duration;
        bool hasErrors;
        Lyrics *lyrics;
} SongData;
#endif

#ifndef PLAYBACKSTATE_STRUCT
#define PLAYBACKSTATE_STRUCT
typedef struct
{
        char filePath[MAXPATHLEN];
        SongData *songdataA;
        SongData *songdataB;
        bool loadA;
        bool loadingFirstDecoder;
        pthread_mutex_t mutex;
        AppState *state;
} LoadingThreadData;
#endif

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
        bool waitingForNext;            // Playlist has songs but playback is stopped.
        bool waitingForPlaylist;        // Playlist is empty.
        bool notifySwitch;              // Emit mpris song switched signal
        bool notifyPlaying;             // Emit mpris music is playing signal
        volatile bool loadedNextSong;
} PlaybackState;

#ifndef USERDATA_STRUCT
#define USERDATA_STRUCT
typedef struct
{
        SongData *songdataA;
        SongData *songdataB;
        bool songdataADeleted;
        bool songdataBDeleted;
        int replayGainCheckFirst;
        SongData *currentSongData;
        ma_uint64 currentPCMFrame;
} UserData;
#endif

#ifndef AUDIODATA_STRUCT
#define AUDIODATA_STRUCT
typedef struct
{
        ma_data_source_base base;
        UserData *pUserData;
        ma_format format;
        ma_uint32 channels;
        ma_uint32 sampleRate;
        ma_uint64 currentPCMFrame;
        ma_uint32 avgBitRate;
        bool switchFiles;
        int currentFileIndex;
        ma_uint64 totalFrames;
        bool endOfListReached;
        bool restart;
} AudioData;
#endif

// --- Getters ---

PlaybackState *getPlaybackState(void);
AudioData *getAudioData(void);
double getPauseSeconds(void);
double getTotalPauseSeconds(void);
void createPlaylist(PlayList **playlist);
Node *getNextSong(void);
Node *getSongToStartFrom(void);
Node *getTryNextSong(void);
PlayList *getPlaylist(void);
PlayList *getUnshuffledPlaylist(void);
PlayList *getFavoritesPlaylist(void);
ProgressBar *getProgressBar(void);
AppState *getAppState(void);
AppSettings *getAppSettings(void);
FileSystemEntry *getLibrary(void);
char *getLibraryFilePath(void);

// --- Setters ---

void setPauseSeconds(double seconds);
void setTotalPauseSeconds(double seconds);
void setNextSong(Node *node);
void setSongToStartFrom(Node *node);
void setTryNextSong(Node *node);
void setAudioData(AudioData *audioData);
void setLibrary(FileSystemEntry *root);
void freePlaylists(void);
void setPlaylist(PlayList *pl);
void setUnshuffledPlaylist(PlayList *pl);
void setFavoritesPlaylist(PlayList *pl);

#endif
