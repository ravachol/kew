#ifndef SETTINGS_H
#define SETTINGS_H

#include <pwd.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <unistd.h>
#include "file.h"
#include "soundcommon.h"
#include "player.h"

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

extern AppSettings settings;

void getConfig(AppSettings *settings);

void setConfig(AppSettings *settings);

#endif
