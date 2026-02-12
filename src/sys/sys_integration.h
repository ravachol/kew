/**
 * @file sys_integration.h
 * @brief System-level integrations and OS interop.
 *
 * Provides hooks for platform-specific features such as
 * power management, clipboard access, or system event handling.
 */

#ifndef SYS_INTEGRATION_H
#define SYS_INTEGRATION_H

#include "common/appstate.h"

#include "gio/gio.h"
#include "glib.h"

/**
 * @brief Sets the global GMainContext.
 *
 * This function sets the global GMainContext to the provided context,
 * allowing for subsequent calls to use the specified context for
 * event processing.
 *
 * @param val The GMainContext to set as global context.
 */
void set_g_main_context(GMainContext *val);

/**
 * @brief Gets the current global GMainContext.
 *
 * This function returns the current global GMainContext, which can
 * be used for event processing in the main loop.
 *
 * @return The current GMainContext.
 */
void *get_g_main_context(void);

/**
 * @brief Sets the global GDBusConnection.
 *
 * This function sets the global GDBusConnection to the provided value,
 * allowing other functions to use this connection for D-Bus communication.
 *
 * @param val The GDBusConnection to set.
 */
void set_gd_bus_connection(GDBusConnection *val);

/**
 * @brief Emits a string property change signal over D-Bus.
 *
 * This function emits a signal to notify the change of a string property.
 * The signal is sent to the MPRIS D-Bus interface to notify listeners of the
 * change.
 *
 * @param property_name The name of the property that has changed.
 * @param new_value The new value of the property.
 */
void emit_string_property_changed(const gchar *property_name, const gchar *new_value);

/**
 * @brief Updates the playback position of the media player.
 *
 * This function updates the playback position by emitting a signal to notify
 * listeners about the current position of the media playback in seconds.
 *
 * @param elapsed_seconds The elapsed playback time in seconds.
 */
void update_playback_position(double elapsed_seconds);

/**
 * @brief Emits a seeked signal when the playback position changes.
 *
 * This function emits a signal to notify listeners that the playback position
 * has been changed after seeking. The new position is given in seconds.
 *
 * @param new_position_seconds The new playback position in seconds.
 */
void emit_seeked_signal(double new_position_seconds);

/**
 * @brief Emits a boolean property change signal over D-Bus.
 *
 * This function emits a signal to notify the change of a boolean property.
 * The signal is sent to the MPRIS D-Bus interface to notify listeners of the
 * change.
 *
 * @param property_name The name of the property that has changed.
 * @param new_value The new value of the property.
 */
void emit_boolean_property_changed(const gchar *property_name, gboolean new_value);

/**
 * @brief Notifies the MPRIS interface about a song change.
 *
 * This function sends metadata about the current song to the MPRIS interface
 * (e.g., title, artist, album) along with the song's length.
 *
 * @param current_song_data A pointer to the `SongData` structure containing the
 *                          metadata and other information about the current song.
 */
void notify_mpris_switch(SongData *current_song_data);

/**
 * @brief Notifies the UI and MPRIS about a song switch.
 *
 * This function updates both the UI and MPRIS interface when a new song starts
 * playing. It sends a notification about the song change and updates the relevant
 * song metadata.
 *
 * @param current_song_data A pointer to the `SongData` structure containing
 *                          the metadata and other information about the new song.
 */
void notify_song_switch(SongData *current_song_data);

/**
 * @brief Processes pending D-Bus events.
 *
 * This function processes any pending events in the GMainContext related to
 * D-Bus communication. It ensures that D-Bus signals are handled in the main
 * event loop.
 */
void process_d_bus_events(void);

/**
 * @brief Resizes the user interface.
 *
 * This function handles resizing the terminal and refreshing the UI. It will
 * ensure that the UI is correctly updated when the terminal size changes.
 *
 * @param uis A pointer to the UIState structure containing UI settings.
 */
void resize(UIState *uis);

/**
 * @brief Ensures only a single instance of kew can run at a time for the current user.
 *
 * This function checks if there is already a running instance of the application
 * by reading the PID file. If a process is found, it sends a signal to it to
 * restart the application. If no process is found, a new instance is started.
 * It creates or updates a PID file to ensure that only one instance runs at a time.
 *
 * @param argv Arguments for restarting the application.
 */
void restart_if_already_running(char *argv[]);

/**
 * @brief Restarts the application by sending a signal to the previous instance.
 *
 * This function attempts to send a SIGUSR1 signal to the previously running
 * instance of the application (identified by the PID in the PID file). If
 * the process is found and successfully stopped, it deletes the PID file and
 * restarts the application by executing it again with the same arguments.
 *
 * @param argv Arguments to restart the application.
 */
void restart_kew(char *argv[]);

/**
 * @brief Initializes the resize signal handling.
 *
 * This function sets up signal handling to respond to terminal window size changes
 * by using SIGWINCH. It configures a signal handler to reset the resize flag when
 * the terminal is resized.
 */
void init_resize(void);

/**
 * @brief Retrieves the global D-Bus connection.
 *
 * This function returns the global GDBusConnection, which is used for communication
 * with the D-Bus system for emitting signals and making method calls.
 *
 * @return A pointer to the current global GDBusConnection.
 */
GDBusConnection *get_gd_bus_connection(void);

/**
 * @brief Exits the program gracefully.
 *
 * This function terminates the application by calling `exit(0)`. It ensures
 * that all registered atexit handlers are run before the program exits.
 */
void quit(void);

#endif
