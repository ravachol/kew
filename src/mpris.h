#ifndef MPRIS_H
#define MPRIS_H

#include <glib.h>
#include <gio/gio.h>
#include "songloader.h"
#include "soundgapless.h"
#include "volume.h"
#include "playlist.h"
#include "playerops.h"

extern GDBusConnection *connection;
extern GMainContext *global_main_context;
extern GMainLoop *main_loop;

void initMpris(void);

void emitStringPropertyChanged(const gchar *propertyName, const gchar *newValue);

void emitBooleanPropertyChanged(const gchar *propertyName, gboolean newValue);

void emitMetadataChanged(const gchar *title, const gchar *artist, const gchar *album, const gchar *coverArtPath, const gchar *trackId, Node *currentSong);

void emitStartPlayingMpris(void);

void emitPlaybackStoppedMpris(void);

void cleanupMpris(void);

#endif
