/**
 * @file trackmanager.[h]
 * @brief Central track indexing and metadata manager.
 *
 * Manages track objects, metadata lookup, and synchronization
 * between the library and active playlists. Provides helper
 * functions for finding and referencing track data.
 */

#include "common/appstate.h"

void loadSong(Node *song, LoadingThreadData *loadingdata, UIState *uis); // FIXME: UI stuff shouldn't be here

int loadFirst(Node *song);

void loadNextSong(void);

void finishLoading(UIState *uis); // FIXME: UI stuff shouldn't be here

void unloadSongA(void);

void unloadSongB(void);

void unloadPreviousSong(void);

void tryLoadNext(void);

void autostartIfStopped(FileSystemEntry *firstEnqueuedEntry, UIState *uis); // FIXME: UI stuff shouldn't be here
