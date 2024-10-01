#ifndef SETTINGS_H
#define SETTINGS_H

#include <pwd.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <unistd.h>
#include "events.h"
#include "file.h"
#include "soundcommon.h"
#include "player.h"
#include "utils.h"

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

#define NUM_KEY_MAPPINGS 48

extern time_t lastTimeAppRan;

extern EventMapping keyMappings[];

extern AppSettings settings;

void getConfig(AppSettings *settings);

void setConfig(AppSettings *settings);

void mapSettingsToKeys(AppSettings *settings, EventMapping *mappings);

#endif
