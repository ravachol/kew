/**
 * @file mpris.h
 * @brief MPRIS (Media Player Remote Interfacing Specification) integration.
 *
 * Implements D-Bus MPRIS interface support for desktop integration,
 * allowing external clients to control playback (e.g., GNOME, KDE, etc.).
 */

#ifndef MPRIS_H
#define MPRIS_H

#include "common/appstate.h"

#include "data/playlist.h"

#include <gio/gio.h>

/**
 * @brief Initializes MPRIS integration and sets up the D-Bus interfaces.
 *
 * This function initializes the MPRIS integration by setting up the necessary
 * D-Bus interfaces that external clients can use to control media playback.
 * It registers the media player with the D-Bus and prepares it for receiving
 * commands and emitting signals related to playback.
 */
void init_mpris(void);

/**
 * @brief Emits a D-Bus signal to notify a change in a string property.
 *
 * This function emits a signal indicating that the specified string property
 * has changed. The change is broadcast to external clients listening for
 * D-Bus property changes.
 *
 * @param property_name The name of the property that has changed.
 * @param new_value The new value of the property.
 */
void emit_string_property_changed(const gchar *property_name, const gchar *new_value);

/**
 * @brief Emits a D-Bus signal to notify a change in a boolean property.
 *
 * This function emits a signal indicating that the specified boolean property
 * has changed. The change is broadcast to external clients listening for
 * D-Bus property changes.
 *
 * @param property_name The name of the property that has changed.
 * @param new_value The new boolean value of the property.
 */
void emit_boolean_property_changed(const gchar *property_name, gboolean new_value);

/**
 * @brief Emits a D-Bus signal when the volume has changed.
 *
 * This function emits a signal indicating that the media player's volume has
 * changed. External clients monitoring the media player's volume can receive
 * this signal.
 */
void emit_volume_changed(void);

/**
 * @brief Emits a D-Bus signal when the shuffle state has changed.
 *
 * This function emits a signal indicating that the shuffle state of the media
 * player has changed (e.g., shuffle enabled or disabled). External clients
 * can use this signal to update their interface accordingly.
 */
void emit_shuffle_changed(void);

/**
 * @brief Emits a D-Bus signal when metadata has changed.
 *
 * This function emits a signal to notify external clients that the metadata
 * (e.g., title, artist, album, cover art, track ID) of the current media track
 * has changed. It also provides the current song and track length.
 *
 * @param title The new title of the track.
 * @param artist The new artist of the track.
 * @param album The new album of the track.
 * @param cover_art_path The path to the cover art image for the track.
 * @param track_id The unique track ID.
 * @param current_song The current song being played.
 * @param length The length of the track in milliseconds.
 */
void emit_metadata_changed(const gchar *title, const gchar *artist, const gchar *album, const gchar *cover_art_path, const gchar *track_id, Node *current_song, gint64 length);

/**
 * @brief Emits a D-Bus signal when playback has started.
 *
 * This function emits a signal indicating that playback has started. It notifies
 * external clients that the media player has begun playing a track.
 */
void emit_start_playing_mpris(void);

/**
 * @brief Emits a D-Bus signal when playback has stopped.
 *
 * This function emits a signal indicating that playback has stopped. It notifies
 * external clients that the media player has ceased playing any track.
 */
void emit_playback_stopped_mpris(void);

/**
 * @brief Cleans up MPRIS integration and D-Bus connections.
 *
 * This function cleans up the MPRIS integration by unregistering D-Bus
 * interfaces and releasing any resources associated with MPRIS functionality.
 * It is called when the media player is shutting down or when MPRIS support
 * is no longer needed.
 */
void cleanup_mpris(void);

#endif
