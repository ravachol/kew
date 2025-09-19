#include <fcntl.h>
#include <glib.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdio.h>
#include "notifications.h"

/*

notifications.c

 Related to desktop notifications.

*/

bool isValidFilepath(const char *path)
{
    if (path == NULL || *path == '\0' || strnlen(path, PATH_MAX) >= PATH_MAX)
    {
        return false;
    }

    int fd = open(path, O_RDONLY);
    bool result = (fd != -1);

    if (fd != -1)
    {
        close(fd);
    }

    return result;
}

void removeBlacklistedChars(const char *input,
                            const char *blacklist,
                            char *output,
                            size_t output_size)
{
        if (!input || !blacklist || !output || output_size == 0)
        {
                if (output && output_size > 0)
                        output[0] = '\0';
                return;
        }

        const char *in_ptr = input;
        char *out_ptr = output;
        size_t chars_copied = 0;

        while (*in_ptr)
        {
                if (chars_copied >= output_size - 1)
                        break;

                if (!strchr(blacklist, *in_ptr))
                {
                        *out_ptr = *in_ptr;
                        out_ptr++;
                        chars_copied++;
                }
                in_ptr++;
        }

        if (chars_copied < output_size)
                output[chars_copied] = '\0';
        else
                output[output_size - 1] = '\0';
}

void ensureNonEmpty(char *str, size_t bufferSize)
{
        if (str == NULL|| bufferSize < 2)
        {
                return;
        }
        if (str[0] == '\0')
        {
                str[0] = ' ';
                str[1] = '\0';
        }
}

#ifdef USE_DBUS

#define NOTIFICATION_INTERVAL_MICROSECONDS 500000 // 0.5 seconds

struct timeval lastNotificationTime = {0, 0};
static char sanitizedArtist[512];
static char sanitizedTitle[512];

static pthread_mutex_t notificationMutex = PTHREAD_MUTEX_INITIALIZER;

int canShowNotification(void)
{
    struct timeval now;
    gettimeofday(&now, NULL);

    // Lock the mutex to prevent race conditions
    pthread_mutex_lock(&notificationMutex);

    // Calculate the elapsed time safely
    long seconds = now.tv_sec - lastNotificationTime.tv_sec;
    long microseconds = now.tv_usec - lastNotificationTime.tv_usec;

    // Handle microsecond wraparound
    if (microseconds < 0) {
        microseconds += 1000000;
        seconds -= 1;
    }

    // Calculate the total elapsed time in microseconds
    long elapsed = seconds * 1000000 + microseconds;

    // Check if the elapsed time is greater than or equal to the notification interval
    if (elapsed >= NOTIFICATION_INTERVAL_MICROSECONDS)
    {
        lastNotificationTime = now; // Update the last notification time
        pthread_mutex_unlock(&notificationMutex);
        return 1;
    }

    pthread_mutex_unlock(&notificationMutex);
    return 0;
}

void onNotificationClosed(void)
{
}

static GDBusConnection *connection = NULL;
static guint last_notification_id = 0;
static guint signal_subscription_id = 0;

static void on_dbus_call_complete(GObject *source_object,
                                  GAsyncResult *res,
                                  gpointer user_data)
{
        GError *error = NULL;
        GVariant *result = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source_object), res, &error);

        if (error)
        {
                fprintf(stderr, "D-Bus call failed or timed out: %s\n", error->message);
                g_error_free(error);
                return;
        }

        // Extract the notification ID from the result
        guint32 *last_notification_id = (guint32 *)user_data;
        g_variant_get(result, "(u)", last_notification_id);
        g_variant_unref(result);
}

static void on_notification_closed_signal(GDBusConnection *connection,
                                          const gchar *sender_name,
                                          const gchar *object_path,
                                          const gchar *interface_name,
                                          const gchar *signal_name,
                                          GVariant *parameters,
                                          gpointer user_data)
{
        (void)connection;
        (void)sender_name;
        (void)object_path;
        (void)interface_name;
        (void)signal_name;
        (void)user_data;

        guint32 id, reason;
        g_variant_get(parameters, "(uu)", &id, &reason);

        if (id == last_notification_id)
        {
                last_notification_id = 0;
        }
}

static void on_close_notification_complete(GObject *source_object,
                                           GAsyncResult *res,
                                           gpointer user_data)
{
        (void)user_data;
        GError *error = NULL;
        GVariant *result = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source_object), res, &error);

        if (error)
        {
                fprintf(stderr, "Failed to close notification: %s\n", error->message);
                g_error_free(error);
        }
        else if (result)
        {
                g_variant_unref(result);
        }

        last_notification_id = 0;
}

typedef struct
{
        GMainLoop *loop;
        gboolean connected;
        GDBusConnection *connection;
        gboolean timeout_triggered;
        int ref_count;
} BusConnectionData;

static void bus_connection_data_ref(BusConnectionData *data)
{
        g_atomic_int_inc(&(data->ref_count));
}

static void bus_connection_data_unref(BusConnectionData *data)
{
        if (g_atomic_int_dec_and_test(&(data->ref_count)))
        {
                g_main_loop_unref(data->loop);
                g_free(data);
        }
}

static gboolean on_timeout(gpointer user_data)
{
        BusConnectionData *data = (BusConnectionData *)user_data;

        if (!data->connected)
        {
                fprintf(stderr, "D-Bus connection timed out.\n");
                data->timeout_triggered = TRUE;
                g_main_loop_quit(data->loop);
        }

        // Decrement reference count
        bus_connection_data_unref(data);

        return FALSE; // Stop the timeout callback from repeating
}

static void on_bus_get_complete(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
        (void)source_object;
        BusConnectionData *data = (BusConnectionData *)user_data;
        GError *error = NULL;

        data->connection = g_bus_get_finish(res, &error);
        if (error)
        {
                fprintf(stderr, "Failed to connect to D-Bus: %s\n", error->message);
                g_error_free(error);
        }
        else
        {
                data->connected = TRUE;
        }

        if (!data->timeout_triggered)
        {
                g_main_loop_quit(data->loop);
        }

        // Decrement reference count
        bus_connection_data_unref(data);
}

GDBusConnection *get_dbus_connection_with_timeout(GBusType bus_type, guint timeout_ms)
{
        // Allocate and initialize the data structure
        BusConnectionData *data = g_new0(BusConnectionData, 1);
        data->loop = g_main_loop_new(NULL, FALSE);
        data->connected = FALSE;
        data->connection = NULL;
        data->timeout_triggered = FALSE;
        data->ref_count = 1; // Start with a single reference

        // Increment reference count for each callback
        bus_connection_data_ref(data); // For on_timeout
        bus_connection_data_ref(data); // For on_bus_get_complete

        // Start the asynchronous bus connection
        g_bus_get(bus_type, NULL, on_bus_get_complete, data);

        // Add a timeout callback
        g_timeout_add(timeout_ms, on_timeout, data);

        // Run the main loop
        g_main_loop_run(data->loop);

        // Store the connection result before cleaning up
        GDBusConnection *connection = data->connection;

        // Decrement reference count for the main loop
        bus_connection_data_unref(data);

        return connection;
}

void cleanupPreviousNotification()
{
        if (last_notification_id != 0)
        {
                // Send CloseNotification call for the active notification
                g_dbus_connection_call(
                    connection,
                    "org.freedesktop.Notifications",
                    "/org/freedesktop/Notifications",
                    "org.freedesktop.Notifications",
                    "CloseNotification",
                    g_variant_new("(u)", last_notification_id),
                    NULL, // No return value expected
                    G_DBUS_CALL_FLAGS_NONE,
                    100,  // Timeout in milliseconds
                    NULL, // Cancellable
                    on_close_notification_complete,
                    NULL);
        }
}

int displaySongNotification(const char *artist, const char *title, const char *cover, UISettings *ui)
{
        if (!ui->allowNotifications || !canShowNotification())
        {
                return 0;
        }

        if (connection == NULL)
        {
                connection = get_dbus_connection_with_timeout(G_BUS_TYPE_SESSION, 100);

                if (connection == NULL)
                {
                        fprintf(stderr, "Failed to connect to session bus\n");
                        return -1;
                }

                signal_subscription_id = g_dbus_connection_signal_subscribe(
                    connection,
                    "org.freedesktop.Notifications",
                    "org.freedesktop.Notifications",
                    "NotificationClosed",
                    "/org/freedesktop/Notifications",
                    NULL,
                    G_DBUS_SIGNAL_FLAGS_NONE,
                    on_notification_closed_signal,
                    NULL,
                    NULL);
        }

        const char *blacklist = "&;|*~<>^()[]{}$\\\"";

        removeBlacklistedChars(artist, blacklist, sanitizedArtist, sizeof(sanitizedArtist));
        removeBlacklistedChars(title, blacklist, sanitizedTitle, sizeof(sanitizedTitle));

        ensureNonEmpty(sanitizedArtist, sizeof(sanitizedTitle));
        ensureNonEmpty(sanitizedTitle, sizeof(sanitizedTitle));

        int coverExists = isValidFilepath(cover);

        cleanupPreviousNotification();

        // Create a new notification
        const gchar *app_name = "kew";
        const gchar *app_icon = (coverExists && cover) ? cover : "";
        const gchar *summary = sanitizedArtist;
        const gchar *body = sanitizedTitle;

        GVariantBuilder actions_builder;
        g_variant_builder_init(&actions_builder, G_VARIANT_TYPE("as"));

        GVariantBuilder hints_builder;
        g_variant_builder_init(&hints_builder, G_VARIANT_TYPE("a{sv}"));

        gint32 expire_timeout = -1;

        g_dbus_connection_call(
            connection,
            "org.freedesktop.Notifications",
            "/org/freedesktop/Notifications",
            "org.freedesktop.Notifications",
            "Notify",
            g_variant_new("(susssasa{sv}i)",
                          app_name,
                          0, // New notification, no replaces_id
                          app_icon,
                          summary,
                          body,
                          &actions_builder,
                          &hints_builder,
                          expire_timeout),
            G_VARIANT_TYPE("(u)"),
            G_DBUS_CALL_FLAGS_NONE,
            100,  // Timeout in milliseconds
            NULL, // Cancellable
            on_dbus_call_complete,
            &last_notification_id);

        return 0;
}

void cleanupDbusConnection()
{
        cleanupPreviousNotification();

        // Unsubscribe from signals
        if (signal_subscription_id != 0)
        {
                g_dbus_connection_signal_unsubscribe(connection, signal_subscription_id);
                signal_subscription_id = 0;
        }

        // Release the connection
        if (connection != NULL)
        {
                g_object_unref(connection);
                connection = NULL;
        }
}
#endif
