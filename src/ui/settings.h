/*
 * @file settings.h
 * @brief Application settings management.
 *
 * Loads, saves, and applies configuration settings such as
 * playback behavior, UI preferences, and library paths.
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include "common/appstate.h"
#include "common/events.h"

#define NUM_DEFAULT_KEY_BINDINGS 52

extern size_t keybinding_count;

TBKeyBinding *get_key_bindings();
void get_config(AppSettings *settings, UISettings *ui);
void get_prefs(AppSettings *settings, UISettings *ui);
void set_config(AppSettings *settings, UISettings *ui);
void set_prefs(AppSettings *settings, UISettings *ui);
void set_path(const char *path);
void map_settings_to_keys(AppSettings *settings, EventMapping *mappings);
int update_rc(const char *path, const char *key, const char *value);
const char *get_binding_string(enum EventType event, bool find_only_one);
AppSettings init_settings(void);

#endif
