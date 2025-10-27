#ifndef PLAYBACK_H
#define PLAYBACK_H

#include "common/appstate.h"

#include <miniaudio.h>
#include <stdbool.h>

void stop_playback(void);
void pause_playback(void);
void toggle_pause_playback(void);
void set_paused(bool val);
void set_stopped(bool val);
bool is_repeat_enabled();
void set_repeat_enabled(bool value);
bool is_paused(void);
bool is_stopped(void);
bool is_playing(void);
bool is_playback_done(void);
void sound_resume_playback(void);

void set_seek_elapsed(double value);
void set_seek_requested(bool value);
void seek_percentage(float percent);
double get_seek_elapsed(void);
float get_seek_percentage(void);
bool is_seek_requested(void);

void set_skip_to_next(bool value);
bool is_EOF_reached(void);
void set_EOF_reached(void);
void set_EOF_handled(void);
void set_impl_switch_reached(void);
void set_impl_switch_not_reached(void);
enum AudioImplementation get_current_implementation_type(void);
void set_current_implementation_type(enum AudioImplementation value);
bool is_skip_to_next(void);
bool is_EOF_reached(void);
bool is_impl_switch_reached(void);

void activate_switch(AudioData *pPCMDataSource);
void execute_switch(AudioData *pPCMDataSource);
ma_device *get_device(void);
void get_current_format_and_sample_rate(ma_format *format, ma_uint32 *sample_rate);

void cleanup_playback_device(void);
int init_playback_device(ma_context *context, ma_format format, ma_uint32 channels, ma_uint32 sample_rate,
                       ma_device *device, ma_device_data_proc dataCallback, void *pUserData);

void m4a_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount);
void opus_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount);
void vorbis_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount);
void webm_on_audio_frames(ma_device *pDevice, void *pFramesOut, const void *pFramesIn, ma_uint32 frameCount);

void clear_current_track(void);
void shutdown_android(void);
void sound_shutdown();

#endif
