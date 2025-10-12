/**
 * @file playback_system.[h]
 * @brief Low-level audio system integration.
 *
 * Manages initialization, configuration, and teardown of
 * audio backends and decoders. Provides the connection between
 * playback operations and sound output subsystems.
 */

#include "common/appstate.h"

int playbackCreate(AppState *state);
void playbackSafeCleanup(AppState *state);
void playbackCleanup(void);
void skip(AppState *state);
void playbackSwitchDecoder(AppState *state);
void playbackShutdown(void);
void playbackUnloadSongs(AppState *state, UserData *userData);
void playbackFreeDecoders(void);
