/**
 * @file discord_rpc.h
 * @brief Discord Rich Presence integration for kew.
 *
 * Provides a lightweight IPC-based integration with Discord to
 * display the current playback status (track title, artist,
 * timestamps, etc.) using Discord Rich Presence.
 *
 * This module manages the lifecycle of the Discord IPC connection
 * and exposes helper functions to notify Discord about playback
 * state changes.
 */

#ifndef DISCORD_RPC_H
#define DISCORD_RPC_H

#include "common/appstate.h"
#include <time.h>

/**
 * @brief Initialize the Discord RPC connection.
 *
 * Attempts to establish a connection to the local Discord IPC socket
 * and performs the initial handshake.
 *
 * This function should be called once during application startup.
 * If Discord is not available, the integration will silently remain
 * disabled until a later reconnection attempt.
 */
void discord_rpc_init(void);

/**
 * @brief Shutdown the Discord RPC connection.
 *
 * Clears any active Rich Presence activity and closes the IPC
 * connection to Discord.
 *
 * This function should be called during application shutdown
 * to ensure proper cleanup of resources.
 */
void discord_rpc_shutdown(void);

/**
 * @brief Notify Discord of a player state change.
 *
 * Updates the Rich Presence activity with the provided song metadata,
 * including:
 * - Track title (shown as "details")
 * - Artist (shown as "name")
 * - Album title (shown as "state")
 * - Playback start and end timestamps
 *
 * If no connection to Discord exists, this function attempts to
 * reconnect automatically.
 *
 * @param song Pointer to the currently playing SongData structure.
 *             Must not be NULL and must contain valid metadata.
 *
 * @param elapsed_seconds Seconds since the song started playing.
 * @param song_duration Duration of the current song in seconds.
 */
void notify_discord_update(SongData *song, time_t elapsed_seconds, time_t song_duration);

/**
 * @brief Notify Discord that playback has paused.
 *
 * Clears the current Rich Presence activity from Discord.
 * If Discord is not currently connected, a reconnection attempt
 * will be made before clearing.
 */
void notify_discord_pause(void);

/**
 * @brief Clear the current Discord Rich Presence activity.
 *
 * Sends a SET_ACTIVITY command without activity data, effectively
 * removing the displayed status from Discord.
 *
 * This function can be used independently when playback stops
 * or when the application wants to explicitly remove presence
 * information.
 */
void discord_rpc_clear(void);

#endif
