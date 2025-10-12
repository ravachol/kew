/**
 * @file playback_ops.[h]
 * @brief Core playback control API.
 *
 * Contains functions to control playback: play, pause, stop, seek,
 * volume adjustments, and track skipping. This module is UI-agnostic
 * and interacts directly with the playback state and audio backends.
 */

#ifndef PLAYBACK_OPS_H
#define PLAYBACK_OPS_H

#include "common/appstate.h"

int play(Node *node, AppState *state);
void stop(AppState *state);
void playbackPlay(AppState *state);
void playbackResumePlayback(AppState *state);
void playbackPause(AppState *state, struct timespec *pause_time);
void playbackStop(AppState *state);
void playbackVolumeChange(int changePercent);
void playbackSeekForward(int seconds);
void playbackSeekBack(int seconds);
void togglePause(AppState *state);
void skipToSong(int id, bool startPlaying, AppState *state);

#endif
