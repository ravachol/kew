
#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

#include <fcntl.h>
#include <glib.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/time.h>
#include "appstate.h"

#ifdef USE_DBUS

int displaySongNotification(const char *artist, const char *title, const char *cover, UISettings *ui);

#endif

void freeLastCover(void);

#endif
