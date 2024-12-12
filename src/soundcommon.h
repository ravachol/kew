#ifndef SOUND_COMMON_H
#define SOUND_COMMON_H

#include <fcntl.h>
#include <glib.h>
#include <math.h>
#include <miniaudio.h>
#include <miniaudio_libopus.h>
#include <miniaudio_libvorbis.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#ifdef USE_FAAD
#include "m4a.h"
#endif

#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include "appstate.h"
#include "file.h"
#include "utils.h"

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

#ifndef MAX_BUFFER_SIZE
#define MAX_BUFFER_SIZE 4800
#endif

#ifndef TAGSETTINGS_STRUCT
#define TAGSETTINGS_STRUCT

#define METADATA_MAX_SIZE 256

typedef struct
{
        char title[METADATA_MAX_SIZE];
        char artist[METADATA_MAX_SIZE];
        char album_artist[METADATA_MAX_SIZE];
        char album[METADATA_MAX_SIZE];
        char date[METADATA_MAX_SIZE];
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
        int coverWidth;
        int coverHeight;
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

enum AudioImplementation
{
        PCM,
        BUILTIN,
        VORBIS,
        OPUS,
        M4A,
        NONE
};

extern volatile bool refresh;

extern AppState appState;

extern AudioData audioData;

extern double duration;

extern double elapsedSeconds;

extern bool hasSilentlySwitched;

extern pthread_mutex_t dataSourceMutex;

extern pthread_mutex_t switchMutex;

enum AudioImplementation getCurrentImplementationType();

void setCurrentImplementationType(enum AudioImplementation value);

int getBufferSize(void);

void setBufferSize(int value);

void setPlayingStatus(bool playing);

bool isPlaying(void);

ma_decoder *getFirstDecoder(void);

ma_decoder *getCurrentBuiltinDecoder(void);

ma_decoder *getPreviousDecoder(void);

ma_format getCurrentFormat(void);

void resetDecoders(void);

ma_libopus *getCurrentOpusDecoder(void);

void resetOpusDecoders(void);

#ifdef USE_FAAD
m4a_decoder *getCurrentM4aDecoder(void);

m4a_decoder *getFirstM4aDecoder(void);
#endif

ma_libopus *getFirstOpusDecoder(void);

ma_libvorbis *getFirstVorbisDecoder(void);

void getVorbisFileInfo(const char *filename, ma_format *format, ma_uint32*channels, ma_uint32 *sampleRate, ma_channel *channelMap);

void getM4aFileInfo(const char *filename, ma_format *format, ma_uint32*channels, ma_uint32 *sampleRate, ma_channel *channelMap);

void getOpusFileInfo(const char *filename, ma_format *format, ma_uint32*channels, ma_uint32 *sampleRate, ma_channel *channelMap);

ma_libvorbis *getCurrentVorbisDecoder(void);

void switchVorbisDecoder(void);

int prepareNextDecoder(char *filepath);

int prepareNextOpusDecoder(char *filepath);

int prepareNextVorbisDecoder(char *filepath);

int prepareNextM4aDecoder(char *filepath);

void resetVorbisDecoders(void);

void resetM4aDecoders(void);

ma_libvorbis *getFirstVorbisDecoder(void);

void getFileInfo(const char* filename, ma_uint32* sampleRate, ma_uint32* channels, ma_format* format);

void initAudioBuffer(void);

ma_int32 *getAudioBuffer(void);

void setAudioBuffer(ma_int32 *buf);

void resetAudioBuffer(void);

void freeAudioBuffer(void);

bool isRepeatEnabled(void);

void setRepeatEnabled(bool value);

bool isShuffleEnabled(void);

void setShuffleEnabled(bool value);

bool isSkipToNext(void);

void setSkipToNext(bool value);

double getSeekElapsed(void);

void setSeekElapsed(double value);

bool isEOFReached(void);

void setEOFReached(void);

void setEOFNotReached(void);

bool isImplSwitchReached(void);

void setImplSwitchReached(void);

void setImplSwitchNotReached(void);

bool isPlaybackDone(void);

float getSeekPercentage(void);

double getPercentageElapsed(void);

bool isSeekRequested(void);

void setSeekRequested(bool value);

void seekPercentage(float percent);

void resumePlayback(void);

void stopPlayback(void);

void pausePlayback(void);

void cleanupPlaybackDevice(void);

void togglePausePlayback(void);

bool isPaused(void);

bool isStopped(void);

void resetDevice(void);

ma_device *getDevice(void);

bool hasBuiltinDecoder(char *filePath);

void setCurrentFileIndex(AudioData *pAudioData, int index);

void activateSwitch(AudioData *pPCMDataSource);

void executeSwitch(AudioData *pPCMDataSource);

gint64 getLengthInMicroSec(double duration);

int getCurrentVolume(void);

void setVolume(int volume);

int adjustVolumePercent(int volumeChange);

void m4a_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount);

void opus_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount);

void vorbis_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount);

void logTime(const char *message);

void clearCurrentTrack(void);

void cleanupDbusConnection();

void freeLastCover(void);

#endif
