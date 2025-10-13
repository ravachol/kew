/**
 * @file notifications.[h]
 * @brief Desktop notifications integration.
 *
 * Sends system notifications for playback events such as
 * song changes, errors, or status updates.
 */

#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

#include "common/appstate.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifdef USE_DBUS

int displaySongNotification(const char *artist, const char *title, const char *cover, UISettings *ui);
void cleanupNotifications(void);

#endif

#endif
