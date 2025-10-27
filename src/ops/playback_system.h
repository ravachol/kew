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

int playback_create(void);
void playback_safe_cleanup(void);
void playback_cleanup(void);
void skip(void);
void playback_switch_decoder(void);
void playback_shutdown(void);
void playback_unload_songs(UserData *user_data);
void playback_free_decoders(void);
void ensure_default_theme_pack();
