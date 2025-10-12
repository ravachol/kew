/**
 * @file trackmanager.[h]
 * @brief Central track indexing and metadata manager.
 *
 * Manages track objects, metadata lookup, and synchronization
 * between the library and active playlists. Provides helper
 * functions for finding and referencing track data.
 */

#include "common/appstate.h"

void loadSong(Node *song, LoadingThreadData *loadingdata, UIState *uis);

int loadFirst(Node *song, AppState *state);

void loadNextSong(AppState *state);

void finishLoading(UIState *uis);

void unloadSongA(AppState *state);

void unloadSongB(AppState *state);

void unloadPreviousSong(AppState *state);

void tryLoadNext(AppState *state);

void autostartIfStopped(FileSystemEntry *firstEnqueuedEntry, UIState *uis);
