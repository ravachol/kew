/**
 * @file discord_rpc.h
 * @brief Integration that shows kew status in discord.
 *
 * Implements a discord RPC integration,

 */
#ifndef DISCORD_RPC_H
#define DISCORD_RPC_H

#include "common/appstate.h"

void discord_rpc_init(void);
void discord_rpc_shutdown(void);

void notify_discord_switch(SongData *song);
void notify_discord_resume(SongData *song);
void notify_discord_pause(void);

void discord_rpc_clear(void);

#endif
