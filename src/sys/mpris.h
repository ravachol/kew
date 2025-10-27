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

void init_mpris(void);
void emit_string_property_changed(const gchar *propertyName, const gchar *newValue);
void emit_boolean_property_changed(const gchar *propertyName, gboolean newValue);
void emit_volume_changed(void);
void emit_shuffle_changed(void);
void emit_metadata_changed(const gchar *title, const gchar *artist, const gchar *album, const gchar *coverArtPath, const gchar *trackId, Node *current_song, gint64 length);
void emit_start_playing_mpris(void);
void emit_playback_stopped_mpris(void);
void cleanup_mpris(void);

#endif
