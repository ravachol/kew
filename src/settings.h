#ifndef SETTINGS_H
#define SETTINGS_H

#include <pwd.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <unistd.h>
#include "appstate.h"
#include "events.h"
#include "file.h"
#include "soundcommon.h"
#include "player.h"
#include "utils.h"

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

extern AppSettings settings;

void getConfig(AppSettings *settings, UISettings *ui);

void setConfig(AppSettings *settings, UISettings *ui);

void mapSettingsToKeys(AppSettings *settings, EventMapping *mappings);

#endif
