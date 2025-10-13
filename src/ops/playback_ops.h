/**
 * @file playback_ops.h
 * @brief Core playback control API.
 *
 * Contains functions to control playback: play, pause, stop, seek,
 * volume adjustments, and track skipping. This module is UI-agnostic
 * and interacts directly with the playback state and audio backends.
 */

#ifndef PLAYBACK_OPS_H
#define PLAYBACK_OPS_H

#include "common/appstate.h"

int playSong(Node *node);
void pauseSong(void);
void play(void);
void stop(void);
void togglePause(void);
void resumePlayback(void);
void volumeChange(int changePercent);
void seek(int seconds);
void skipToSong(int id, bool startPlaying);

#endif
