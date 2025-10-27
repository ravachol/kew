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

int play_song(Node *node);
void pause_song(void);
void play(void);
void stop(void);
void ops_toggle_pause(void);
void resume_playback(void);
void volume_change(int change_percent);
void seek(int seconds);
void skip_to_song(int id, bool start_playing);

#endif
