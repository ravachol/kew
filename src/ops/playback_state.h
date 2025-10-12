/**
 * @file playback_state.[h]
 * @brief Maintains playback runtime state.
 *
 * Stores and manages the current playback status, elapsed time,
 * current song pointer, and related flags (paused, stopped, etc.).
 * Provides accessors and mutators for playback state data.
 */

#include "common/appstate.h"

#include <stdbool.h>

// Getters
bool playbackIsRepeatEnabled(void);
bool playbackIsRepeatListEnabled(void);
bool playbackIsShuffleEnabled(void);
bool playbackIsPaused(void);
bool playbackIsStopped(void);
bool playbackisDone(void);
bool playbackIsEof(void);
bool isCurrentSongDeleted(void);
bool playbackIsImplSwitchReached(void);
UserData *playbackGetUserData(void);
SongData *getCurrentSongData(void);
double getCurrentSongDuration(void);
bool determineCurrentSongData(SongData **currentSongData);

// Setters
void playbackSetRepeatEnabled(bool enabled);
void playbackSetRepeatListEnabled(bool value);
void playbackSetShuffleEnabled(bool value);
void playbackSetEofHandled(void);

