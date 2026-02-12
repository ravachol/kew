/**
 * @file sound.[h]
 * @brief Playback related functions.
 *
 * Provides an api for stopping, starting and so on.
 * and switching decoders.
 */

#ifndef PLAYBACK_H
#define PLAYBACK_H

#include "common/appstate.h"

#include <miniaudio.h>
#include <stdbool.h>

/**
 * @brief Stops audio playback.
 *
 * This function halts the playback immediately. Any currently playing audio
 * will be stopped, and the playback state will be set to stopped.
 */
void stop_playback(void);


/**
 * @brief Pauses audio playback.
 *
 * This function pauses the audio playback without stopping it. It can be resumed
 * later from the same point.
 */
void pause_playback(void);


/**
 * @brief Toggles the pause state of audio playback.
 *
 * This function either pauses or resumes playback, depending on the current
 * playback state. If the audio is currently playing, it will pause. If it is
 * paused, it will resume.
 */
void toggle_pause_playback(void);


/**
 * @brief Sets the paused state of the playback.
 *
 * @param val Boolean indicating whether playback should be paused (true) or resumed (false).
 */
void set_paused(bool val);


/**
 * @brief Sets the stopped state of the playback.
 *
 * @param val Boolean indicating whether playback should be stopped (true) or not.
 */
void set_stopped(bool val);


/**
 * @brief Checks if repeat mode is enabled.
 *
 * @return `true` if repeat is enabled, otherwise `false`.
 */
bool pb_is_repeat_enabled(void);


/**
 * @brief Enables or disables repeat mode.
 *
 * @param value Boolean indicating whether repeat mode should be enabled (true) or disabled (false).
 */
void set_repeat_enabled(bool value);


/**
 * @brief Checks if the playback is currently paused.
 *
 * @return `true` if the playback is paused, otherwise `false`.
 */
bool pb_is_paused(void);


/**
 * @brief Checks if the playback is currently stopped.
 *
 * @return `true` if the playback is stopped, otherwise `false`.
 */
bool pb_is_stopped(void);


/**
 * @brief Checks if the audio is currently playing.
 *
 * @return `true` if audio is playing, otherwise `false`.
 */
bool is_playing(void);


/**
 * @brief Resumes playback from the current position.
 *
 * This function resumes the audio playback if it was previously paused.
 */
void sound_resume_playback(void);


/**
 * @brief Sets the elapsed time for seeking.
 *
 * @param value The seek time (in seconds) to set.
 */
void set_seek_elapsed(double value);


/**
 * @brief Sets whether a seek request is made.
 *
 * @param value Boolean indicating whether a seek request is made (true) or not (false).
 */
void set_seek_requested(bool value);


/**
 * @brief Seeks to a percentage of the total track duration.
 *
 * @param percent The percentage of the total track to seek to (0 to 100).
 */
void seek_percentage(float percent);


/**
 * @brief Gets the elapsed seek time.
 *
 * @return The seek time in seconds.
 */
double get_seek_elapsed(void);


/**
 * @brief Gets the seek percentage.
 *
 * @return The seek percentage (0 to 100).
 */
float get_seek_percentage(void);


/**
 * @brief Checks if a seek request has been made.
 *
 * @return `true` if a seek request has been made, otherwise `false`.
 */
bool is_seek_requested(void);


/**
 * @brief Sets whether to skip to the next track.
 *
 * @param value Boolean indicating whether to skip to the next track (true) or not (false).
 */
void set_skip_to_next(bool value);


/**
 * @brief Checks if the end of the file (EOF) has been reached.
 *
 * @return `true` if EOF is reached, otherwise `false`.
 */
bool pb_is_EOF_reached(void);


/**
 * @brief Sets the EOF state as reached.
 *
 * This function marks the end of the file (EOF) as reached. It signals that
 * there are no more tracks to play.
 */
void set_EOF_reached(void);


/**
 * @brief Marks the EOF as handled.
 *
 * This function is used to mark that the EOF has been processed and can be reset.
 */
void pb_set_EOF_handled(void);


/**
 * @brief Sets whether the implementation switch has been reached.
 *
 * This function marks the point where the audio implementation switch is complete.
 */
void set_impl_switch_reached(void);


/**
 * @brief Sets whether the implementation switch has not been reached.
 *
 * This function marks the point where the audio implementation switch has not been completed.
 */
void set_impl_switch_not_reached(void);


/**
 * @brief Gets the current audio implementation type.
 *
 * @return The current audio implementation type as an enum value of `AudioImplementation`.
 */
enum AudioImplementation get_current_implementation_type(void);


/**
 * @brief Sets the current audio implementation type.
 *
 * @param value The new audio implementation type to set.
 */
void set_current_implementation_type(enum AudioImplementation value);


/**
 * @brief Checks whether skip to next is enabled.
 *
 * @return `true` if skip to next is enabled, otherwise `false`.
 */
bool is_skip_to_next(void);


/**
 * @brief Checks if the implementation switch has been reached.
 *
 * @return `true` if the implementation switch is reached, otherwise `false`.
 */
bool pb_is_impl_switch_reached(void);


/**
 * @brief Activates a switch between two audio data sources.
 *
 * This function activates the switch between two audio data sources, typically
 * used when changing the audio implementation or transitioning between tracks.
 *
 * @param p_pcm_data_source Pointer to the audio data source.
 */
void activate_switch(AudioData *p_pcm_data_source);


/**
 * @brief Executes the audio data source switch.
 *
 * This function executes the process of switching between audio data sources.
 *
 * @param p_pcm_data_source Pointer to the audio data source.
 */
void execute_switch(AudioData *p_pcm_data_source);


/**
 * @brief Retrieves the current audio device.
 *
 * @return Pointer to the current `ma_device` object.
 */
ma_device *get_device(void);


/**
 * @brief Retrieves the current audio format and sample rate.
 *
 * @param format Pointer to a variable where the audio format will be stored.
 * @param sample_rate Pointer to a variable where the sample rate will be stored.
 */
void get_current_format_and_sample_rate(ma_format *format, ma_uint32 *sample_rate);


/**
 * @brief Cleans up the playback device resources.
 *
 * This function frees any resources associated with the playback device,
 * ensuring the device is properly cleaned up and ready for shutdown.
 */
void pb_cleanup_playback_device(void);


/**
 * @brief Initializes the playback device.
 *
 * @param context Pointer to the `ma_context` object.
 * @param format The desired audio format.
 * @param channels The number of audio channels.
 * @param sample_rate The sample rate of the audio.
 * @param device Pointer to the `ma_device` object to initialize.
 * @param data_callback The callback function for processing audio data.
 * @param pUserData Pointer to user-defined data to pass to the callback.
 *
 * @return A status code indicating success or failure.
 */
int init_playback_device(ma_context *context, ma_format format, ma_uint32 channels, ma_uint32 sample_rate,
                         ma_device *device, ma_device_data_proc data_callback, void *pUserData);


/**
 * @brief Handles audio frames for M4A playback.
 *
 * This callback function processes audio frames for M4A file playback. It is
 * used to provide data to the audio device during the playback of an M4A file.
 *
 * @param p_device Pointer to the audio device.
 * @param p_frames_out Pointer to the output audio frames buffer.
 * @param p_frames_in Pointer to the input audio frames buffer.
 * @param frame_count The number of frames to process.
 */
void m4a_on_audio_frames(ma_device *p_device, void *p_frames_out, const void *p_frames_in, ma_uint32 frame_count);


/**
 * @brief Handles audio frames for Opus playback.
 *
 * This callback function processes audio frames for Opus file playback. It is
 * used to provide data to the audio device during the playback of an Opus file.
 *
 * @param p_device Pointer to the audio device.
 * @param p_frames_out Pointer to the output audio frames buffer.
 * @param p_frames_in Pointer to the input audio frames buffer.
 * @param frame_count The number of frames to process.
 */
void opus_on_audio_frames(ma_device *p_device, void *p_frames_out, const void *p_frames_in, ma_uint32 frame_count);


/**
 * @brief Handles audio frames for Vorbis playback.
 *
 * This callback function processes audio frames for Vorbis file playback. It is
 * used to provide data to the audio device during the playback of a Vorbis file.
 *
 * @param p_device Pointer to the audio device.
 * @param p_frames_out Pointer to the output audio frames buffer.
 * @param p_frames_in Pointer to the input audio frames buffer.
 * @param frame_count The number of frames to process.
 */
void vorbis_on_audio_frames(ma_device *p_device, void *p_frames_out, const void *p_frames_in, ma_uint32 frame_count);

/**
 * @brief Handles audio frames for WebM playback.
 *
 * This callback function processes audio frames for WebM file playback. It is
 * used to provide data to the audio device during the playback of a WebM file.
 *
 * @param p_device Pointer to the audio device.
 * @param p_frames_out Pointer to the output audio frames buffer.
 * @param p_frames_in Pointer to the input audio frames buffer.
 * @param frame_count The number of frames to process.
 */
void webm_on_audio_frames(ma_device *p_device, void *p_frames_out, const void *p_frames_in, ma_uint32 frame_count);


/**
 * @brief Clears the current track from memory.
 *
 * This function releases any resources associated with the current track and
 * prepares for playing the next track or stopping playback.
 */
void clear_current_track(void);


/**
 * @brief Shuts down the Android-specific audio components.
 *
 * This function is responsible for shutting down any Android-specific audio
 * resources or state when the application is terminating or pausing.
 */
void shutdown_android(void);


/**
 * @brief Shuts down the audio system.
 *
 * This function cleans up the audio system, releases resources, and stops all
 * active audio playback or processing tasks.
 */
void pb_sound_shutdown(void);

#endif
