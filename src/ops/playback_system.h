/**
 * @file playback_system.h
 * @brief Low-level audio system integration.
 *
 * Manages initialization, configuration, and teardown of
 * audio backends and decoders. Provides the connection between
 * playback operations and sound output subsystems.
 */

#include "common/appstate.h"

#include "data/theme.h"

int playbackCreate(void);
void playbackSafeCleanup(void);
void playbackCleanup(void);
void skip(void);
void playbackSwitchDecoder(void);
void playbackShutdown(void);
void playbackUnloadSongs(UserData *userData);
void playbackFreeDecoders(void);
void ensureDefaultThemePack();
