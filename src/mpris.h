#ifndef MPRIS_H
#define MPRIS_H

#include <gio/gio.h>
#include <glib.h>
#include "playerops.h"
#include "playlist.h"
#include "sound.h"
#include "soundcommon.h"

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
