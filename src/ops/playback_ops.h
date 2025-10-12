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

int play(Node *node);
void stop(void);
void playbackPlay(void);
void playbackResumePlayback(void);
void playbackPause(struct timespec *pause_time);
void playbackStop(void);
void playbackVolumeChange(int changePercent);
void playbackSeekForward(int seconds);
void playbackSeekBack(int seconds);
void togglePause(void);
void skipToSong(int id, bool startPlaying);

#endif
