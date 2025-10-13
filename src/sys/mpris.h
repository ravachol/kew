/**
 * @file mpris.[h]
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

void initMpris(void);
void emitStringPropertyChanged(const gchar *propertyName, const gchar *newValue);
void emitBooleanPropertyChanged(const gchar *propertyName, gboolean newValue);
void emitVolumeChanged(void);
void emitShuffleChanged(void);
void emitMetadataChanged(const gchar *title, const gchar *artist, const gchar *album, const gchar *coverArtPath, const gchar *trackId, Node *currentSong, gint64 length);
void emitStartPlayingMpris(void);
void emitPlaybackStoppedMpris(void);
void cleanupMpris(void);

#endif
