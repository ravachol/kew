/**
 * @file notifications.c
 * @brief Desktop notifications integration.
 *
 * Sends system notifications for playback events such as
 * song changes, errors, or status updates.
 */

#include "notifications.h"

#include "utils/k_log.h"

#include <fcntl.h>
#include <glib.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

bool is_valid_filepath(const char *path)
{
        if (path == NULL || *path == '\0' || strnlen(path, KEW_PATH_MAX) >= KEW_PATH_MAX)
                return false;

        struct stat st;
        return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int utf8_seq_len(const char *s)
{
        unsigned char c = (unsigned char)*s;
        int len = 1;

        if ((c & 0xe0) == 0xc0)
                len = 2;
        else if ((c & 0xf0) == 0xe0)
                len = 3;
        else if ((c & 0xf8) == 0xf0)
                len = 4;

        // Don't step past the terminator on a truncated sequence
        for (int i = 1; i < len; i++)
                if (s[i] == '\0')
                        return i;

        return len;
}

void remove_blacklisted_chars(
    const char *input,
    const char *blacklist,
    char *output,
    size_t output_size)
{
        if (!input || !blacklist || !output || output_size < 2) {
                if (output && output_size > 0)
                        output[0] = '\0';
                return;
        }

        const char *in_ptr = input;
        char *out_ptr = output;
        size_t chars_copied = 0;

        while (*in_ptr && chars_copied < output_size - 1) {
                int len = utf8_seq_len(in_ptr);

                bool blacklisted = false;
                const char *bl_ptr = blacklist;
                while (*bl_ptr) {
                        int bl_len = utf8_seq_len(bl_ptr);
                        if (len == bl_len && memcmp(in_ptr, bl_ptr, len) == 0) {
                                blacklisted = true;
                                break;
                        }
                        bl_ptr += bl_len;
                }

                if (!blacklisted) {
                        // Don't write half a character when the buffer is nearly full
                        if (chars_copied + len > output_size - 1)
                                break;
                        memcpy(out_ptr, in_ptr, len);
                        out_ptr += len;
                        chars_copied += len;
                }
                in_ptr += len;
        }

        *out_ptr = '\0';
}

void ensure_non_empty(char *str, size_t buffer_size)
{
        if (str == NULL || buffer_size < 2) {
                return;
        }
        if (str[0] == '\0') {
                str[0] = ' ';
                str[1] = '\0';
        }
}

#ifdef USE_DBUS

static char sanitized_artist[512];
static char sanitized_title[512];

static GDBusConnection *connection = NULL;
static guint last_notification_id = 0;
static guint signal_subscription_id = 0;

static void on_dbus_call_complete(GObject *source_object,
                                  GAsyncResult *res,
                                  gpointer user_data)
{
        GError *error = NULL;
        GVariant *result = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source_object), res, &error);

        if (error) {
                k_log("D-Bus call failed or timed out: %s\n", error->message);
                g_error_free(error);
                return;
        }

        // Extract the notification ID from the result
        guint32 *out_id = (guint32 *)user_data;
        g_variant_get(result, "(u)", out_id);
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

        if (id == last_notification_id) {
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

        if (error) {
                k_log("Failed to close notification: %s\n", error->message);
                g_error_free(error);
        } else if (result) {
                g_variant_unref(result);
        }

        last_notification_id = 0;
}

typedef struct
{
        GMainLoop *loop;
        gboolean connected;
        GDBusConnection *connection;
        GCancellable *cancellable;
        gboolean timeout_triggered;
        int ref_count;
} BusConnectionData;

static void bus_connection_data_ref(BusConnectionData *data)
{
        g_atomic_int_inc(&(data->ref_count));
}

static void bus_connection_data_unref(BusConnectionData *data)
{
        if (g_atomic_int_dec_and_test(&(data->ref_count))) {
                g_main_loop_unref(data->loop);
                g_clear_object(&data->cancellable);
                g_free(data);
        }
}

static gboolean on_timeout(gpointer user_data)
{
        k_log("on_timeout: entered\n");

        BusConnectionData *data = user_data;

        if (!data->connected) {
                k_log("on_timeout: D-Bus connection timed out.\n");
                data->timeout_triggered = TRUE;
                g_cancellable_cancel(data->cancellable);
                g_main_loop_quit(data->loop);
                k_log("on_timeout: called g_main_loop_quit()\n");
        }

        return FALSE;
}

static void on_bus_get_complete(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
        (void)source_object;
        BusConnectionData *data = (BusConnectionData *)user_data;
        GError *error = NULL;

        data->connection = g_bus_get_finish(res, &error);
        if (error) {
                k_log("Failed to connect to D-Bus: %s\n", error->message);
                g_error_free(error);
        } else {
                data->connected = TRUE;
        }

        if (!data->timeout_triggered) {
                g_main_loop_quit(data->loop);
        }

        // Decrement reference count
        bus_connection_data_unref(data);
}

GDBusConnection *get_dbus_connection_with_timeout(GBusType bus_type, guint timeout_ms)
{
        BusConnectionData *data = g_new0(BusConnectionData, 1);
        GMainContext *context = g_main_context_new();

        data->loop = g_main_loop_new(context, FALSE);
        data->ref_count = 1; // Start with a single reference

        // Increment reference count for each callback
        bus_connection_data_ref(data); // For on_timeout
        bus_connection_data_ref(data); // For on_bus_get_complete

        g_main_context_push_thread_default(context);

        data->cancellable = g_cancellable_new();

        // Start the asynchronous bus connection
        g_bus_get(bus_type, data->cancellable, on_bus_get_complete, data);

        g_main_context_pop_thread_default(context);

        // Add a timeout callback
        GSource *source = g_timeout_source_new(timeout_ms);
        g_source_set_callback(source, on_timeout, data, (GDestroyNotify)bus_connection_data_unref);
        g_source_attach(source, context);

        // Run the main loop
        g_main_loop_run(data->loop);

        while (g_main_context_pending(context))
                g_main_context_iteration(context, FALSE);

        // Store the connection result before cleaning up
        GDBusConnection *connection = data->connection;

        // Remove source from context and run its destroy notify
        g_source_destroy(source);
        g_source_unref(source);

        bus_connection_data_unref(data);
        g_main_context_unref(context);

        return connection;
}

void cleanup_previous_notification()
{
        if (last_notification_id != 0) {
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

int display_song_notification(const char *artist, const char *title, const char *cover, UISettings *ui)
{
        if (!ui || !ui->allowNotifications) {
                return 0;
        }

        if (connection == NULL) {
                connection = get_dbus_connection_with_timeout(G_BUS_TYPE_SESSION, 200);

                if (connection == NULL) {
                        k_log("Failed to connect to session bus\n");
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

        const char *blacklist = "&<>";

        remove_blacklisted_chars(artist, blacklist, sanitized_artist, sizeof(sanitized_artist));
        remove_blacklisted_chars(title, blacklist, sanitized_title, sizeof(sanitized_title));

        ensure_non_empty(sanitized_artist, sizeof(sanitized_artist));
        ensure_non_empty(sanitized_title, sizeof(sanitized_title));

        int coverExists = is_valid_filepath(cover);

        // Create a new notification
        const gchar *app_name = "kew";
        const gchar *app_icon = coverExists ? cover : "";
        const gchar *summary = sanitized_title;
        const gchar *body = sanitized_artist;

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
                          (guint32)last_notification_id, // replace the previous one
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

#endif

void notifications_shutdown()
{
#ifdef USE_DBUS
        if (connection != NULL && last_notification_id != 0) {
                GVariant *r = g_dbus_connection_call_sync(
                    connection,
                    "org.freedesktop.Notifications",
                    "/org/freedesktop/Notifications",
                    "org.freedesktop.Notifications",
                    "CloseNotification",
                    g_variant_new("(u)", last_notification_id),
                    NULL, G_DBUS_CALL_FLAGS_NONE, 100, NULL, NULL);
                if (r)
                        g_variant_unref(r);
                last_notification_id = 0;
        }

        // Unsubscribe from signals
        if (signal_subscription_id != 0) {
                g_dbus_connection_signal_unsubscribe(connection, signal_subscription_id);
                signal_subscription_id = 0;
        }

        // Release the connection
        if (connection != NULL) {
                g_object_unref(connection);
                connection = NULL;
        }
#endif
}
