
#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

#include "appstate.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifdef USE_DBUS

int displaySongNotification(const char *artist, const char *title, const char *cover, UISettings *ui);

#endif

void freeLastCover(void);

#endif
