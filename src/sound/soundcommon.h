/**
 * @file soundcommon.[h]
 * @brief Shared sound utilities and common decoder helpers.
 *
 * Contains functions and definitions used by multiple sound backends,
 * including initialization, buffer management, and format conversion.
 */

#ifndef SOUND_COMMON_H
#define SOUND_COMMON_H

#ifdef USE_FAAD

#include "m4a.h"

#endif

#include "webm.h"

#include "common/appstate.h"

#include <miniaudio.h>
#include <miniaudio_libopus.h>
#include <miniaudio_libvorbis.h>

#include <fcntl.h>
#include <glib.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

#ifndef MAX_BUFFER_SIZE
#define MAX_BUFFER_SIZE 32768
#endif

#ifndef MAX_DECODERS
#define MAX_DECODERS 2
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

struct m4a_decoder;

typedef struct m4a_decoder m4a_decoder;

typedef void (*uninit_func)(void *decoder);

enum AudioImplementation getCurrentImplementationType();

void setBufferReady(bool val);
void setCurrentImplementationType(enum AudioImplementation value);
void setBufferSize(int value);
void setPlayingStatus(bool playing);
void getCurrentFormatAndSampleRate(ma_format *format, ma_uint32 *sampleRate);
void resetAllDecoders(void);
void getM4aFileInfo(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sampleRate, ma_channel *channelMap, int *avgBitRate, k_m4adec_filetype *fileType);
void getVorbisFileInfo(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sampleRate, ma_channel *channelMap);
void getOpusFileInfo(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sampleRate, ma_channel *channelMap);
void switchVorbisDecoder(void);
void soundResumePlayback(void);
void getFileInfo(const char *filename, ma_uint32 *sampleRate, ma_uint32 *channels, ma_format *format);
void initAudioBuffer(void);
void *getAudioBuffer(void);
void setAudioBuffer(void *buf, int numSamples, ma_uint32 sampleRate, ma_uint32 channels, ma_format format);
void resetAudioBuffer(void);
void freeAudioBuffer(void);
void setSeekElapsed(double value);
void setRepeatListEnabled(bool value);
void setShuffleEnabled(bool value);
void setSkipToNext(bool value);
void setEOFReached(void);
void setEOFNotReached(void);
void setImplSwitchReached(void);
void setImplSwitchNotReached(void);
void setSeekRequested(bool value);
void seekPercentage(float percent);
void stopPlayback(void);
void pausePlayback(void);
void cleanupPlaybackDevice(void);
void togglePausePlayback(void);
void setPaused(bool val);
void setStopped(bool val);
void setRepeatEnabled(bool value);
void setCurrentFileIndex(AudioData *pAudioData, int index);
void activateSwitch(AudioData *pPCMDataSource);
void executeSwitch(AudioData *pPCMDataSource);
void setVolume(int volume);
void m4a_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount);
void opus_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount);
void vorbis_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount);
void logTime(const char *message);
void clearCurrentTrack(void);
void getWebmFileInfo(const char *filename, ma_format *format, ma_uint32 *channels, ma_uint32 *sampleRate, ma_channel *channelMap);
void webm_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount);
void shutdownAndroid(void);
bool isRepeatEnabled(void);
bool isImplSwitchReached(void);
bool isPlaybackDone(void);
bool isPaused(void);
bool isStopped(void);
bool isSeekRequested(void);
bool hasBuiltinDecoder(char *filePath);
bool doesOSallowVolumeControl(void);
bool isBufferReady(void);
bool isPlaying(void);
bool isEOFReached(void);
bool isRepeatListEnabled(void);
bool isShuffleEnabled(void);
bool isSkipToNext(void);
int initPlaybackDevice(ma_context *context, ma_format format, ma_uint32 channels, ma_uint32 sampleRate,
                       ma_device *device, ma_device_data_proc dataCallback, void *pUserData);
int getCurrentVolume(void);
int adjustVolumePercent(int volumeChange);
int prepareNextWebmDecoder(SongData *songData);
int getFftSize(void);
int getBufferSize(void);
int prepareNextDecoder(char *filepath);
int prepareNextOpusDecoder(char *filepath);
int prepareNextVorbisDecoder(char *filepath);
int prepareNextM4aDecoder(SongData *songData);
double getSeekElapsed(void);
float getSeekPercentage(void);
ma_decoder *getFirstDecoder(void);
ma_decoder *getCurrentBuiltinDecoder(void);
ma_decoder *getPreviousDecoder(void);
ma_libopus *getCurrentOpusDecoder(void);
#ifdef USE_FAAD
m4a_decoder *getCurrentM4aDecoder(void);
m4a_decoder *getFirstM4aDecoder(void);
#endif
ma_libopus *getFirstOpusDecoder(void);
ma_libvorbis *getFirstVorbisDecoder(void);
ma_libvorbis *getCurrentVorbisDecoder(void);
ma_libvorbis *getFirstVorbisDecoder(void);
int32_t unpack_s24(const ma_uint8 *p);
ma_device *getDevice(void);
ma_webm *getCurrentWebmDecoder(void);
ma_webm *getFirstWebmDecoder(void);
ma_result callReadPCMFrames(
    ma_data_source *pDataSource,
    ma_format format,
    void *pFramesOut,
    ma_uint64 framesRead,
    ma_uint32 channels,
    ma_uint64 remainingFrames,
    ma_uint64 *pFramesToRead);

#endif
