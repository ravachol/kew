/**
 * @file notifications.h
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

/**
 * @brief Displays a notification for the current song being played.
 *
 * This function displays a notification with information about the current song,
 * including the artist name, song title, and album cover (if available).
 * It also ensures that notifications are only shown if allowed by the user and
 * if notifications are supported by the system.
 *
 * The function will sanitize the artist and title strings to remove any blacklisted
 * characters and will check if the cover image path is valid. It subscribes to
 * notification closed signals and handles notification cleanup if necessary.
 *
 * @param artist The name of the artist for the song.
 * @param title The title of the song.
 * @param cover The file path to the album cover image.
 * @param ui A pointer to the `UISettings` structure containing user interface preferences.
 *
 * @return 0 if the notification was displayed successfully, or -1 if there was an error.
 */
int display_song_notification(const char *artist, const char *title, const char *cover, UISettings *ui);

/**
 * @brief Cleans up and unsubscribes from notification-related resources.
 *
 * This function cleans up any active notifications and unsubscribes from
 * the notification signals to prevent memory leaks. It also releases the D-Bus
 * connection used for notifications.
 *
 * It ensures that the system is no longer receiving or handling notifications
 * once the player is no longer active.
 */
void cleanup_notifications(void);

#endif

#endif
