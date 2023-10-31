#ifndef MPRIS_H
#define MPRIS_H

#include <glib.h>
#include <gio/gio.h>
#include "songloader.h"
#include "soundgapless.h"
#include "playerops.h"
#include "volume.h"
#include "playlist.h"

extern GMainContext *global_main_context;
extern GMainLoop *main_loop;

void initMpris();

void emitStartPlayingMpris();

void emitPlaybackStoppedMpris();

void cleanupMpris();

#endif