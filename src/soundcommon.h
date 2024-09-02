#ifndef SOUND_COMMON_H
#define SOUND_COMMON_H

#include <FreeImage.h>
#include <glib.h>
#include <miniaudio.h>
#include <miniaudio_libopus.h>
#include <miniaudio_libvorbis.h>
#include <sys/wait.h>
#include "m4a.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include "file.h"
#include "utils.h"

#ifdef USE_LIBNOTIFY
#include <libnotify/notify.h>
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

#ifndef MAX_BUFFER_SIZE
#define MAX_BUFFER_SIZE 4800
#endif

#ifndef TAGSETTINGS_STRUCT
#define TAGSETTINGS_STRUCT

typedef struct
{
        char title[256];
        char artist[256];
        char album_artist[256];
        char album[256];
        char date[256];
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
        FIBITMAP *cover;
        double duration;
        bool hasErrors;
} SongData;

#endif

#ifndef USERDATA_STRUCT
#define USERDATA_STRUCT
typedef struct
{
        SongData *songdataA;
        SongData *songdataB;
        bool songdataADeleted;
        bool songdataBDeleted;
        SongData *currentSongData;
        ma_uint32 currentPCMFrame;
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
        ma_uint32 currentPCMFrame;
        bool switchFiles;
        int currentFileIndex;
        ma_uint64 totalFrames;
        bool endOfListReached;
        bool restart;
} AudioData;
#endif

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
        char coverEnabled[2];
        char coverAnsi[2];
        char useProfileColors[2];
        char visualizerEnabled[2];
        char visualizerHeight[6];
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
        char toggleColorsDerivedFrom[6];
        char toggleVisualizer[6];
        char toggleAscii[6];
        char toggleRepeat[6];
        char toggleShuffle[6];
        char seekBackward[6];
        char seekForward[6];
        char savePlaylist[6];
        char addToMainPlaylist[6];
        char updateLibrary[6];
        char quit[6];
        char hardQuit[6];
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
        char hardNextPage[6];
        char hardPrevPage[6];
        char hardRemove[6];
        char lastVolume[12];
        char allowNotifications[2];
        char color[2];       
        char artistColor[2];
        char enqueuedColor[2];
        char titleColor[2];
        char hideLogo[2];
        char hideHelp[2];
        char cacheLibrary[6];
} AppSettings;

#endif

enum AudioImplementation
{
        PCM,
        BUILTIN,
        VORBIS,
        OPUS,
        M4A,
        NONE
};

typedef enum {
    SONG_VIEW,
    KEYBINDINGS_VIEW,
    PLAYLIST_VIEW,
    LIBRARY_VIEW,
    SEARCH_VIEW
} ViewState;

typedef struct {
    ViewState currentView;
} AppState;

#ifdef USE_LIBNOTIFY
extern NotifyNotification *previous_notification;
#endif

extern AppState appState;

extern bool allowNotifications;

extern volatile bool refresh;

extern AudioData audioData;

extern double duration;

extern bool doQuit;

extern double elapsedSeconds;

extern bool hasSilentlySwitched;

extern pthread_mutex_t dataSourceMutex;

extern pthread_mutex_t switchMutex;

enum AudioImplementation getCurrentImplementationType();

void setCurrentImplementationType(enum AudioImplementation value);

int getBufferSize();

void setBufferSize(int value);

void setPlayingStatus(bool playing);

bool isPlaying();

ma_decoder *getFirstDecoder();

ma_decoder *getCurrentBuiltinDecoder();

ma_decoder *getPreviousDecoder();

ma_format getCurrentFormat();

void resetDecoders();

ma_libopus *getCurrentOpusDecoder();

void resetOpusDecoders();

m4a_decoder *getCurrentM4aDecoder();

m4a_decoder *getFirstM4aDecoder();

ma_libopus *getFirstOpusDecoder();

ma_libvorbis *getFirstVorbisDecoder();

void getVorbisFileInfo(const char *filename, ma_format *format, ma_uint32*channels, ma_uint32 *sampleRate, ma_channel *channelMap);

void getM4aFileInfo(const char *filename, ma_format *format, ma_uint32*channels, ma_uint32 *sampleRate, ma_channel *channelMap);

void getOpusFileInfo(const char *filename, ma_format *format, ma_uint32*channels, ma_uint32 *sampleRate, ma_channel *channelMap);

ma_libvorbis *getCurrentVorbisDecoder();

void switchVorbisDecoder();

int prepareNextOpusDecoder(char *filepath);

int prepareNextOpusDecoder(char *filepath);

int prepareNextVorbisDecoder(char *filepath);

int prepareNextM4aDecoder(char *filepath);

void resetVorbisDecoders();

void resetM4aDecoders();

ma_libvorbis *getFirstVorbisDecoder();

void getFileInfo(const char* filename, ma_uint32* sampleRate, ma_uint32* channels, ma_format* format);

void initAudioBuffer();

ma_int32 *getAudioBuffer();

void setAudioBuffer(ma_int32 *buf);

void resetAudioBuffer();

void freeAudioBuffer();

bool isRepeatEnabled();

void setRepeatEnabled(bool value);

bool isShuffleEnabled();

void setShuffleEnabled(bool value);

bool isSkipToNext();

void setSkipToNext(bool value);

double getSeekElapsed();

void setSeekElapsed(double value);

bool isEOFReached();

void setEOFReached();

void setEOFNotReached();

bool isImplSwitchReached();

void setImplSwitchReached();

void setImplSwitchNotReached();

bool isPlaybackDone();

float getSeekPercentage();

double getPercentageElapsed();

bool isSeekRequested();

void setSeekRequested(bool value);

void seekPercentage(float percent);

void resumePlayback();

void stopPlayback();

void pausePlayback();

void cleanupPlaybackDevice();

void togglePausePlayback();

bool isPaused();

bool isStopped();

void resetDevice();

ma_device *getDevice();

bool hasBuiltinDecoder(char *filePath);

void setCurrentFileIndex(AudioData *pAudioData, int index);

void activateSwitch(AudioData *pPCMDataSource);

void executeSwitch(AudioData *pPCMDataSource);

gint64 getLengthInMicroSec(double duration);

int displaySongNotification(const char *artist, const char *title, const char *cover);

int getCurrentVolume(void);

void setVolume(int volume);

int adjustVolumePercent(int volumeChange);

void m4a_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount);

void opus_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount);

void vorbis_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount);

void logTime(const char *message);

void clearCurrentTrack();

#endif
