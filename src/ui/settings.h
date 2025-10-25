/*
 * @file settings.h
 * @brief Application settings management.
 *
 * Loads, saves, and applies configuration settings such as
 * playback behavior, UI preferences, and library paths.
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include "common/events.h"
#include "common/appstate.h"

#define NUM_DEFAULT_KEY_BINDINGS 50

extern size_t keybindingCount;

TBKeyBinding *getKeyBindings();
void getConfig(AppSettings *settings, UISettings *ui);
void getPrefs(AppSettings *settings, UISettings *ui);
void setConfig(AppSettings *settings, UISettings *ui);
void setPrefs(AppSettings *settings, UISettings *ui);
void setPath(const char *path);
void mapSettingsToKeys(AppSettings *settings, UISettings *ui, EventMapping *mappings);
int updateRc(const char *path, const char *key, const char *value);
const char* getBindingString(enum EventType event);
AppSettings initSettings(void);

#endif
