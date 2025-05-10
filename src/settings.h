#ifndef SETTINGS_H
#define SETTINGS_H

#include "events.h"
#include "appstate.h"


#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

#ifndef NUM_KEY_MAPPINGS
#define NUM_KEY_MAPPINGS 65
#endif

extern AppSettings settings;

void getConfig(AppSettings *settings, UISettings *ui);

void setConfig(AppSettings *settings, UISettings *ui);

void mapSettingsToKeys(AppSettings *settings, UISettings *ui, EventMapping *mappings);

#endif
