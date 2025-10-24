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

#if defined(__ANDROID__) || defined(__APPLE__)
#define NUM_DEFAULT_KEY_BINDINGS 55
#else
#define NUM_DEFAULT_KEY_BINDINGS 50
#endif

extern size_t keybindingCount;

TBKeyBinding *getKeyBindings();
void getConfig(AppSettings *settings, UISettings *ui);
void getPrefs(AppSettings *settings, UISettings *ui);
void setConfig(AppSettings *settings, UISettings *ui);
void setPrefs(AppSettings *settings, UISettings *ui);
void setPath(const char *path);
void mapSettingsToKeys(AppSettings *settings, UISettings *ui, EventMapping *mappings);
int updateRc(const char *path, const char *key, const char *value);
AppSettings initSettings(void);

#endif
