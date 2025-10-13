/**
 * @file settings.[h]
 * @brief Application settings management.
 *
 * Loads, saves, and applies configuration settings such as
 * playback behavior, UI preferences, and library paths.
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include "common/events.h"
#include "common/appstate.h"

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

void getConfig(AppSettings *settings, UISettings *ui);
void setConfig(AppSettings *settings, UISettings *ui);
void mapSettingsToKeys(AppSettings *settings, UISettings *ui, EventMapping *mappings);
void initSettings(AppSettings *settings);

#endif
