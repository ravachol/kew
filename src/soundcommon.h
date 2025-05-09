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
#include "webm.h"

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

#ifndef MAX_BUFFER_SIZE
#define MAX_BUFFER_SIZE 32768
#endif

#ifndef MAX_DECODERS
#define MAX_DECODERS 2
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
        int replayGainCheckFirst;
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
        ma_uint32 avgBitRate;
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
        WEBM,
        NONE
};

extern int hopSize;
extern int fftSize;
extern int prevFftSize;

typedef void (*uninit_func)(void *decoder);

extern AppState appState;

extern AudioData audioData;

extern bool bufferReady;

extern double elapsedSeconds;

extern bool hasSilentlySwitched;

extern pthread_mutex_t dataSourceMutex;

extern pthread_mutex_t switchMutex;

extern bool paused;

extern bool stopped;

extern ma_device device;

enum AudioImplementation getCurrentImplementationType();

void setCurrentImplementationType(enum AudioImplementation value);

int getBufferSize(void);

void setBufferSize(int value);

void setPlayingStatus(bool playing);

bool isPlaying(void);

ma_decoder *getFirstDecoder(void);

ma_decoder *getCurrentBuiltinDecoder(void);

ma_decoder *getPreviousDecoder(void);

void getCurrentFormatAndSampleRate(ma_format *format, ma_uint32 *sampleRate);

void resetAllDecoders();

ma_libopus *getCurrentOpusDecoder(void);

#ifdef USE_FAAD
m4a_decoder *getCurrentM4aDecoder(void);

m4a_decoder *getFirstM4aDecoder(void);

void getM4aFileInfo(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sampleRate, ma_channel *channelMap, int *avgBitRate, k_m4adec_filetype *fileType);
#endif

ma_libopus *getFirstOpusDecoder(void);

ma_libvorbis *getFirstVorbisDecoder(void);

void getVorbisFileInfo(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sampleRate, ma_channel *channelMap);

void getOpusFileInfo(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sampleRate, ma_channel *channelMap);

ma_libvorbis *getCurrentVorbisDecoder(void);

void switchVorbisDecoder(void);

int prepareNextDecoder(char *filepath);

int prepareNextOpusDecoder(char *filepath);

int prepareNextVorbisDecoder(char *filepath);

int prepareNextM4aDecoder(SongData *songData);

ma_libvorbis *getFirstVorbisDecoder(void);

void getFileInfo(const char *filename, ma_uint32 *sampleRate, ma_uint32 *channels, ma_format *format);

void initAudioBuffer(void);

void *getAudioBuffer(void);

void setAudioBuffer(void *buf, int numSamples, ma_uint32 sampleRate, ma_uint32 channels, ma_format format);

int32_t unpack_s24(const ma_uint8* p);

void resetAudioBuffer(void);

void freeAudioBuffer(void);

bool isRepeatEnabled(void);

void setRepeatEnabled(bool value);

bool isRepeatListEnabled(void);

void setRepeatListEnabled(bool value);

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

void getWebmFileInfo(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sampleRate, ma_channel *channelMap);

int prepareNextWebmDecoder(SongData *songData);

ma_webm *getCurrentWebmDecoder(void);

ma_webm *getFirstWebmDecoder(void);

void webm_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount);

ma_result callReadPCMFrames(
    ma_data_source *pDataSource,
    ma_format format,
    void *pFramesOut,
    ma_uint64 framesRead,
    ma_uint32 channels,
    ma_uint64 remainingFrames,
    ma_uint64 *pFramesToRead);

#endif
