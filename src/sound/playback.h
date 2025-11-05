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
bool pb_is_repeat_enabled();
void set_repeat_enabled(bool value);
bool pb_is_paused(void);
bool pb_is_stopped(void);
bool is_playing(void);
void sound_resume_playback(void);

void set_seek_elapsed(double value);
void set_seek_requested(bool value);
void seek_percentage(float percent);
double get_seek_elapsed(void);
float get_seek_percentage(void);
bool is_seek_requested(void);

void set_skip_to_next(bool value);
bool pb_is_EOF_reached(void);
void set_EOF_reached(void);
void pb_set_EOF_handled(void);
void set_impl_switch_reached(void);
void set_impl_switch_not_reached(void);
enum AudioImplementation get_current_implementation_type(void);
void set_current_implementation_type(enum AudioImplementation value);
bool is_skip_to_next(void);
bool pb_is_EOF_reached(void);
bool pb_is_impl_switch_reached(void);

void activate_switch(AudioData *p_pcm_data_source);
void execute_switch(AudioData *p_pcm_data_source);
ma_device *get_device(void);
void get_current_format_and_sample_rate(ma_format *format, ma_uint32 *sample_rate);

void pb_cleanup_playback_device(void);
int init_playback_device(ma_context *context, ma_format format, ma_uint32 channels, ma_uint32 sample_rate,
                         ma_device *device, ma_device_data_proc data_callback, void *pUserData);

void m4a_on_audio_frames(ma_device *p_device, void *p_frames_out, const void *p_frames_in, ma_uint32 frame_count);
void opus_on_audio_frames(ma_device *p_device, void *p_frames_out, const void *p_frames_in, ma_uint32 frame_count);
void vorbis_on_audio_frames(ma_device *p_device, void *p_frames_out, const void *p_frames_in, ma_uint32 frame_count);
void webm_on_audio_frames(ma_device *p_device, void *p_frames_out, const void *p_frames_in, ma_uint32 frame_count);

void clear_current_track(void);
void shutdown_android(void);
void pb_sound_shutdown();

#endif
