/**
 * @file macos_nowplaying.h
 * @brief macOS Now Playing integration via MediaPlayer framework.
 *
 * Provides hooks for the macOS Now Playing info center and remote
 * command center, enabling media key support, Control Center display,
 * AirPods controls, and Touch Bar integration on macOS.
 */

#ifndef MACOS_NOWPLAYING_H
#define MACOS_NOWPLAYING_H

/**
 * @brief Initializes macOS Now Playing integration.
 *
 * Sets up the NSApplication instance (required for receiving remote
 * command events in a terminal application) and registers handlers
 * for play, pause, toggle, stop, next, previous, and seek commands
 * with MPRemoteCommandCenter.
 */
void init_macos_nowplaying(void);

/**
 * @brief Cleans up macOS Now Playing integration.
 *
 * Removes all remote command targets and clears the Now Playing info.
 */
void cleanup_macos_nowplaying(void);

/**
 * @brief Sets the Now Playing metadata for the current track.
 *
 * Updates the system Now Playing info center with track metadata,
 * which appears in Control Center, the menu bar, Touch Bar, and
 * on connected Bluetooth devices.
 *
 * @param title Track title.
 * @param artist Track artist.
 * @param album Album name.
 * @param cover_art_path File path to album artwork (may be NULL or empty).
 * @param duration Track duration in seconds.
 */
void macos_set_now_playing_info(const char *title, const char *artist,
                                const char *album, const char *cover_art_path,
                                double duration);

/**
 * @brief Updates the elapsed playback position.
 *
 * @param position_seconds Current playback position in seconds.
 */
void macos_update_playback_position(double position_seconds);

/**
 * @brief Sets the playback state to playing.
 */
void macos_set_playback_state_playing(void);

/**
 * @brief Sets the playback state to paused.
 */
void macos_set_playback_state_paused(void);

/**
 * @brief Sets the playback state to stopped.
 */
void macos_set_playback_state_stopped(void);

/**
 * @brief Processes pending macOS application events.
 *
 * Must be called periodically from the main event loop to receive
 * remote command callbacks from the system (media keys, AirPods, etc.).
 */
void macos_process_events(void);

#endif
