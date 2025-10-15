/**
 * @file playback_state.h
 * @brief Maintains playback runtime state.
 *
 * Stores and manages the current playback status, elapsed time,
 * current song pointer, and related flags (paused, stopped, etc.).
 * Provides accessors and mutators for playback state data.
 */

#include "common/appstate.h"

#include <stdbool.h>

// Getters
bool isRepeatListEnabled(void);
bool isShuffleEnabled(void);
bool opsIsRepeatEnabled(void);
bool opsIsPaused(void);
bool opsIsStopped(void);
bool opsIsDone(void);
bool opsIsEof(void);
bool isCurrentSongDeleted(void);
bool opsIsImplSwitchReached(void);
bool determineCurrentSongData(SongData **currentSongData);
int getVolume();
double getCurrentSongDuration(void);
void getFormatAndSampleRate(ma_format *format, ma_uint32 *sampleRate);
UserData *opsGetUserData(void);
SongData *getCurrentSongData(void);

// Setters
void setRepeatEnabled(bool enabled);
void setRepeatListEnabled(bool value);
void setShuffleEnabled(bool value);
void opsSetEofHandled(void);

