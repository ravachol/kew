#ifndef SETTINGS_H
#define SETTINGS_H

#include "events.h"
#include "appstate.h"


#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

void getConfig(AppSettings *settings, UISettings *ui);

void setConfig(AppSettings *settings, UISettings *ui);

void mapSettingsToKeys(AppSettings *settings, UISettings *ui, EventMapping *mappings);

#endif
