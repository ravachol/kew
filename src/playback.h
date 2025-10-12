#ifndef PLAYBACK_OPS_H
#define PLAYBACK_OPS_H

#include "appstate.h"

int play(Node *node, AppState *state);

void playbackPlay(AppState *state);

void playbackPause(AppState *state, struct timespec *pause_time);

void togglePause(AppState *state);

void stop(AppState *state);

void seekForward(UIState *uis);

void seekBack(UIState *uis);

struct timespec getPauseTime(void);

double getElapsedSeconds(void);

void skip(AppState *state);

void loadSong(Node *song, LoadingThreadData *loadingdata, UIState *uis); // FIXME probably shouldn't be exposed

int loadFirst(Node *song, AppState *state); // FIXME probably shouldn't be exposed

SongData *getCurrentSongData(void);

void autostartIfStopped(FileSystemEntry *firstEnqueuedEntry, UIState *uis);

#endif
