#ifndef PLAYBACK_H
#define PLAYBACK_H

#include "common/appstate.h"

#include <miniaudio.h>
#include <stdbool.h>

void stopPlayback(void);
void pausePlayback(void);
void togglePausePlayback(void);
void setPaused(bool val);
void setStopped(bool val);
bool isRepeatEnabled();
void setRepeatEnabled(bool value);
bool isPaused(void);
bool isStopped(void);
bool isPlaying(void);
bool isPlaybackDone(void);
void soundResumePlayback(void);

void setSeekElapsed(double value);
void setSeekRequested(bool value);
void seekPercentage(float percent);
double getSeekElapsed(void);
float getSeekPercentage(void);
bool isSeekRequested(void);

void setSkipToNext(bool value);
bool isEOFReached(void);
void setEofReached(void);
void setEofHandled(void);
void setImplSwitchReached(void);
void setImplSwitchNotReached(void);
enum AudioImplementation getCurrentImplementationType(void);
void setCurrentImplementationType(enum AudioImplementation value);
bool isSkipToNext(void);
bool isEOFReached(void);
bool isImplSwitchReached(void);

void activateSwitch(AudioData *pPCMDataSource);
void executeSwitch(AudioData *pPCMDataSource);
ma_device *getDevice(void);
void getCurrentFormatAndSampleRate(ma_format *format, ma_uint32 *sampleRate);

void cleanupPlaybackDevice(void);
int initPlaybackDevice(ma_context *context, ma_format format, ma_uint32 channels, ma_uint32 sampleRate,
                       ma_device *device, ma_device_data_proc dataCallback, void *pUserData);

void m4a_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount);
void opus_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount);
void vorbis_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount);
void webm_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount);

void clearCurrentTrack(void);
void shutdownAndroid(void);

#endif
