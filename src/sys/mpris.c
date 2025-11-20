/**
 * @file mpris.c
 * @brief MPRIS (Media Player Remote Interfacing Specification) integration.
 *
 * Implements D-Bus MPRIS interface support for desktop integration,
 * allowing external clients to control playback (e.g., GNOME, KDE, etc.).
 */

#include "mpris.h"

#include "sys_integration.h"

#include "ui/control_ui.h"

#include "ops/playback_clock.h"
#include "ops/playback_ops.h"
#include "ops/playback_state.h"
#include "ops/playlist_ops.h"

#include "sound/playback.h"
#include "sound/volume.h"

#include <glib.h>
#include <math.h>

#ifndef __APPLE__

static guint registration_id;
static guint bus_name_id;
static guint player_registration_id;
static const gchar *loop_status = "None";
static gdouble rate = 1.0;
static gdouble volume = 0.5;
static gdouble minimum_rate = 1.0;
static gdouble maximum_rate = 1.0;
static gboolean can_go_next = TRUE;
static gboolean can_go_previous = TRUE;
static gboolean can_play = TRUE;
static gboolean can_pause = TRUE;
static gboolean can_seek = FALSE;
static gboolean can_control = TRUE;

#define MAX_STATUS_LEN 64

const gchar *introspection_xml =
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection "
    "1.0//EN\" "
    "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
    "<node>\n"
    "  <interface name=\"org.mpris.MediaPlayer2\">\n"
    "    <method name=\"Raise\"/>\n"
    "    <method name=\"Quit\"/>\n"
    "    <property name=\"CanQuit\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"Fullscreen\" type=\"b\" access=\"readwrite\"/>\n"
    "    <property name=\"CanSetFullscreen\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"CanRaise\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"HasTrackList\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"Identity\" type=\"s\" access=\"read\"/>\n"
    "    <property name=\"DesktopEntry\" type=\"s\" access=\"read\"/>\n"
    "    <property name=\"DesktopIconName\" type=\"s\" access=\"read\"/>\n"
    "    <property name=\"SupportedUriSchemes\" type=\"as\" access=\"read\"/>\n"
    "    <property name=\"SupportedMimeTypes\" type=\"as\" access=\"read\"/>\n"
    "  </interface>\n"
    "  <interface name=\"org.mpris.MediaPlayer2.Player\">\n"
    "    <method name=\"Next\"/>\n"
    "    <method name=\"Previous\"/>\n"
    "    <method name=\"Pause\"/>\n"
    "    <method name=\"PlayPause\"/>\n"
    "    <method name=\"Stop\"/>\n"
    "    <method name=\"Play\"/>\n"
    "    <method name=\"Seek\">\n"
    "      <arg name=\"x\" type=\"x\" direction=\"in\"/>\n"
    "    </method>\n"
    "    <method name=\"SetPosition\">\n"
    "      <arg name=\"o\" type=\"o\" direction=\"in\"/>\n"
    "      <arg name=\"x\" type=\"x\" direction=\"in\"/>\n"
    "    </method>\n"
    "    <method name=\"OpenUri\">\n"
    "      <arg name=\"s\" type=\"s\" direction=\"in\"/>\n"
    "    </method>\n"
    "    <signal name=\"Seeked\">\n"
    "      <arg name=\"x\" type=\"x\"/>\n"
    "    </signal>\n"
    "    <property name=\"PlaybackStatus\" type=\"s\" access=\"read\"/>\n"
    "    <property name=\"LoopStatus\" type=\"s\" access=\"readwrite\"/>\n"
    "    <property name=\"Rate\" type=\"d\" access=\"readwrite\"/>\n"
    "    <property name=\"Shuffle\" type=\"b\" access=\"readwrite\"/>\n"
    "    <property name=\"Metadata\" type=\"a{sv}\" access=\"read\"/>\n"
    "    <property name=\"Volume\" type=\"d\" access=\"readwrite\"/>\n"
    "    <property name=\"Position\" type=\"x\" access=\"read\"/>\n"
    "    <property name=\"MinimumRate\" type=\"d\" access=\"read\"/>\n"
    "    <property name=\"MaximumRate\" type=\"d\" access=\"read\"/>\n"
    "    <property name=\"CanGoNext\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"CanGoPrevious\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"CanPlay\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"CanPause\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"CanSeek\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"CanControl\" type=\"b\" access=\"read\"/>\n"
    "  </interface>\n"
    "</node>\n";

static const gchar *identity = "kew";
static const gchar *desktop_icon_name = ""; // Without file extension
static const gchar *desktop_entry = "";     // The name of your .desktop file

static void handle_raise(GDBusConnection *connection, const gchar *sender,
                         const gchar *object_path, const gchar *interface_name,
                         const gchar *method_name, GVariant *parameters,
                         GDBusMethodInvocation *invocation, gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)method_name;
        (void)parameters;
        (void)invocation;
        (void)user_data;

        g_dbus_method_invocation_return_value(invocation, NULL);
}

static void handle_quit(GDBusConnection *connection, const gchar *sender,
                        const gchar *object_path, const gchar *interface_name,
                        const gchar *method_name, GVariant *parameters,
                        GDBusMethodInvocation *invocation, gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)method_name;
        (void)parameters;
        (void)invocation;
        (void)user_data;

        quit();
}

static gboolean get_identity(GDBusConnection *connection, const gchar *sender,
                             const gchar *object_path,
                             const gchar *interface_name,
                             const gchar *property_name, GVariant **value,
                             GError **error, gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)property_name;
        (void)error;
        (void)user_data;

        *value = g_variant_new_string(identity);
        return TRUE;
}

static gboolean get_desktop_entry(GDBusConnection *connection,
                                  const gchar *sender, const gchar *object_path,
                                  const gchar *interface_name,
                                  const gchar *property_name, GVariant **value,
                                  GError **error, gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)property_name;
        (void)error;
        (void)user_data;

        *value = g_variant_new_string(desktop_entry);
        return TRUE;
}

static gboolean
get_desktop_icon_name(GDBusConnection *connection, const gchar *sender,
                      const gchar *object_path, const gchar *interface_name,
                      const gchar *property_name, GVariant **value,
                      GError **error, gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)property_name;
        (void)error;
        (void)user_data;

        *value = g_variant_new_string(desktop_icon_name);
        return TRUE;
}

static void handle_next(GDBusConnection *connection, const gchar *sender,
                        const gchar *object_path, const gchar *interface_name,
                        const gchar *method_name, GVariant *parameters,
                        GDBusMethodInvocation *invocation, gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)method_name;
        (void)parameters;
        (void)user_data;

        skip_to_next_song();
        g_dbus_method_invocation_return_value(invocation, NULL);
}

static void handle_previous(GDBusConnection *connection, const gchar *sender,
                            const gchar *object_path,
                            const gchar *interface_name,
                            const gchar *method_name, GVariant *parameters,
                            GDBusMethodInvocation *invocation,
                            gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)method_name;
        (void)parameters;
        (void)user_data;

        skip_to_prev_song();
        g_dbus_method_invocation_return_value(invocation, NULL);
}

static void handle_pause(GDBusConnection *connection, const gchar *sender,
                         const gchar *object_path, const gchar *interface_name,
                         const gchar *method_name, GVariant *parameters,
                         GDBusMethodInvocation *invocation, gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)method_name;
        (void)parameters;
        (void)invocation;
        (void)user_data;

        pause_song();
        g_dbus_method_invocation_return_value(invocation, NULL);
}

static void handle_play_pause(GDBusConnection *connection, const gchar *sender,
                              const gchar *object_path,
                              const gchar *interface_name,
                              const gchar *method_name, GVariant *parameters,
                              GDBusMethodInvocation *invocation,
                              gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)method_name;
        (void)parameters;
        (void)user_data;

        ops_toggle_pause();

        g_dbus_method_invocation_return_value(invocation, NULL);
}

static void handle_stop(GDBusConnection *connection, const gchar *sender,
                        const gchar *object_path, const gchar *interface_name,
                        const gchar *method_name, GVariant *parameters,
                        GDBusMethodInvocation *invocation, gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)method_name;
        (void)parameters;
        (void)user_data;

        if (!pb_is_stopped())
                stop();

        g_dbus_method_invocation_return_value(invocation, NULL);
}

static void handle_play(GDBusConnection *connection, const gchar *sender,
                        const gchar *object_path, const gchar *interface_name,
                        const gchar *method_name, GVariant *parameters,
                        GDBusMethodInvocation *invocation, gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)method_name;
        (void)parameters;
        (void)invocation;
        (void)user_data;

        play();

        g_dbus_method_invocation_return_value(invocation, NULL);
}

static void handle_seek(GDBusConnection *connection, const gchar *sender,
                        const gchar *object_path, const gchar *interface_name,
                        const gchar *method_name, GVariant *parameters,
                        GDBusMethodInvocation *invocation, gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)method_name;
        (void)user_data;

        gint64 offset;
        g_variant_get(parameters, "(x)", &offset);

        gboolean success = seek_position(offset, get_current_song_duration());

        if (success) {
                g_dbus_method_invocation_return_value(invocation, NULL);
        } else {
                g_dbus_method_invocation_return_error(
                    invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                    "Failed to seek to position");
        }
}

static void handle_set_position(GDBusConnection *connection,
                                const gchar *sender, const gchar *object_path,
                                const gchar *interface_name,
                                const gchar *method_name, GVariant *parameters,
                                GDBusMethodInvocation *invocation,
                                gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)method_name;
        (void)user_data;

        const gchar *track_id;
        gint64 new_position;

        // - "o" is an object path (or track identifier)
        // - "x" is a 64-bit integer representing the position
        g_variant_get(parameters, "(&ox)", &track_id, &new_position);

        gboolean success = set_position(new_position, get_current_song_duration());

        if (success) {
                // If setting the position was successful, return success with
                // no additional value
                g_dbus_method_invocation_return_value(invocation, NULL);
        } else {
                // If setting the position failed, return an error
                g_dbus_method_invocation_return_error(
                    invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                    "Failed to set position for track %s", track_id);
        }
}
#endif

#ifndef __APPLE__
static void handle_method_call(GDBusConnection *connection, const gchar *sender,
                               const gchar *object_path,
                               const gchar *interface_name,
                               const gchar *method_name, GVariant *parameters,
                               GDBusMethodInvocation *invocation,
                               gpointer user_data)
{
        if (g_strcmp0(method_name, "PlayPause") == 0) {
                handle_play_pause(connection, sender, object_path,
                                  interface_name, method_name, parameters,
                                  invocation, user_data);
        } else if (g_strcmp0(method_name, "Next") == 0) {
                handle_next(connection, sender, object_path, interface_name,
                            method_name, parameters, invocation, user_data);
        } else if (g_strcmp0(method_name, "Previous") == 0) {
                handle_previous(connection, sender, object_path, interface_name,
                                method_name, parameters, invocation, user_data);
        } else if (g_strcmp0(method_name, "Pause") == 0) {
                handle_pause(connection, sender, object_path, interface_name,
                             method_name, parameters, invocation, user_data);
        } else if (g_strcmp0(method_name, "Stop") == 0) {
                handle_stop(connection, sender, object_path, interface_name,
                            method_name, parameters, invocation, user_data);
        } else if (g_strcmp0(method_name, "Play") == 0) {
                handle_play(connection, sender, object_path, interface_name,
                            method_name, parameters, invocation, user_data);
        } else if (g_strcmp0(method_name, "Seek") == 0) {
                handle_seek(connection, sender, object_path, interface_name,
                            method_name, parameters, invocation, user_data);
        } else if (g_strcmp0(method_name, "SetPosition") == 0) {
                handle_set_position(connection, sender, object_path,
                                    interface_name, method_name, parameters,
                                    invocation, user_data);
        } else if (g_strcmp0(method_name, "Raise") == 0) {
                handle_raise(connection, sender, object_path, interface_name,
                             method_name, parameters, invocation, user_data);
        } else if (g_strcmp0(method_name, "Quit") == 0) {
                handle_quit(connection, sender, object_path, interface_name,
                            method_name, parameters, invocation, user_data);
        } else {
                g_dbus_method_invocation_return_dbus_error(
                    invocation, "org.freedesktop.DBus.Error.UnknownMethod",
                    "No such method");
        }
}
#endif

#ifndef __APPLE__
static void on_bus_name_acquired(GDBusConnection *connection, const gchar *name,
                                 gpointer user_data)
{
        (void)connection;
        (void)name;
        (void)user_data;
}

static void on_bus_name_lost(GDBusConnection *connection, const gchar *name,
                             gpointer user_data)
{
        (void)connection;
        (void)name;
        (void)user_data;
}

static gboolean
get_playback_status(GDBusConnection *connection, const gchar *sender,
                    const gchar *object_path, const gchar *interface_name,
                    const gchar *property_name, GVariant **value,
                    GError **error, gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)property_name;
        (void)error;
        (void)user_data;

        const gchar *status = "Stopped";

        if (pb_is_paused()) {
                status = "Paused";
        } else if (get_current_song() == NULL || pb_is_stopped()) {
                status = "Stopped";
        } else {
                status = "Playing";
        }
        *value = g_variant_new_string(status);
        return TRUE;
}

static gboolean get_loop_status(GDBusConnection *connection,
                                const gchar *sender, const gchar *object_path,
                                const gchar *interface_name,
                                const gchar *property_name, GVariant **value,
                                GError **error, gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)property_name;
        (void)error;
        (void)user_data;

        *value = g_variant_new_string(loop_status);
        return TRUE;
}

static gboolean get_rate(GDBusConnection *connection, const gchar *sender,
                         const gchar *object_path, const gchar *interface_name,
                         const gchar *property_name, GVariant **value,
                         GError **error, gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)property_name;
        (void)error;
        (void)user_data;

        *value = g_variant_new_double(rate);
        return TRUE;
}

static gboolean get_shuffle(GDBusConnection *connection, const gchar *sender,
                            const gchar *object_path,
                            const gchar *interface_name,
                            const gchar *property_name, GVariant **value,
                            GError **error, gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)property_name;
        (void)error;
        (void)user_data;

        *value = g_variant_new_boolean(is_shuffle_enabled() ? TRUE : FALSE);
        return TRUE;
}

static gboolean get_metadata(GDBusConnection *connection, const gchar *sender,
                             const gchar *object_path,
                             const gchar *interface_name,
                             const gchar *property_name, GVariant **value,
                             GError **error, gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)property_name;
        (void)error;
        (void)user_data;

        SongData *current_song_data = get_current_song_data();

        GVariantBuilder metadata_builder;
        g_variant_builder_init(&metadata_builder, G_VARIANT_TYPE_DICTIONARY);

        if (get_current_song() != NULL && current_song_data != NULL &&
            current_song_data->metadata != NULL) {
                g_variant_builder_add(
                    &metadata_builder, "{sv}", "xesam:title",
                    g_variant_new_string(current_song_data->metadata->title));

                // Build list of strings for artist
                const gchar *artist_list_storage[2];
                artist_list_storage[0] = (g_strcmp0(current_song_data->metadata->artist, "")  == 0) ? "" : current_song_data->metadata->artist;
                artist_list_storage[1] = NULL;

                g_variant_builder_add(&metadata_builder, "{sv}", "xesam:artist",
                      g_variant_new_strv(artist_list_storage, -1));

                gchar *coverArtUrl =
                    g_strdup_printf("file://%s", current_song_data->cover_art_path);

                g_variant_builder_add(
                    &metadata_builder, "{sv}", "xesam:album",
                    g_variant_new_string(current_song_data->metadata->album));
                g_variant_builder_add(
                    &metadata_builder, "{sv}", "xesam:contentCreated",
                    g_variant_new_string(current_song_data->metadata->date));
                g_variant_builder_add(&metadata_builder, "{sv}", "mpris:artUrl",
                                      g_variant_new_string(coverArtUrl));
                g_variant_builder_add(
                    &metadata_builder, "{sv}", "mpris:trackid",
                    g_variant_new_object_path(current_song_data->track_id));

                gint64 length =
                    llround(current_song_data->duration * G_USEC_PER_SEC);
                g_variant_builder_add(&metadata_builder, "{sv}", "mpris:length",
                                      g_variant_new_int64(length));

                g_free(coverArtUrl);
        } else {
                g_variant_builder_add(&metadata_builder, "{sv}", "xesam:title",
                                      g_variant_new_string(""));

                static const gchar *empty_artist_list[] = { "", NULL };
                g_variant_builder_add(
                    &metadata_builder, "{sv}", "xesam:artist",
                    g_variant_new_strv(empty_artist_list, -1));

                g_variant_builder_add(&metadata_builder, "{sv}", "xesam:album",
                                      g_variant_new_string(""));
                g_variant_builder_add(&metadata_builder, "{sv}",
                                      "xesam:contentCreated",
                                      g_variant_new_string(""));
                g_variant_builder_add(&metadata_builder, "{sv}", "mpris:artUrl",
                                      g_variant_new_string(""));
                g_variant_builder_add(
                    &metadata_builder, "{sv}", "mpris:trackid",
                    g_variant_new_object_path(
                        "/org/mpris/MediaPlayer2/TrackList/NoTrack"));

                gint64 placeholderLength = 0;
                g_variant_builder_add(&metadata_builder, "{sv}", "mpris:length",
                                      g_variant_new_int64(placeholderLength));
        }

        GVariant *metadata_variant = g_variant_builder_end(&metadata_builder);
        *value = g_variant_ref_sink(metadata_variant);
        return TRUE;
}

static gboolean get_volume_mpris(GDBusConnection *connection, const gchar *sender,
                                 const gchar *object_path,
                                 const gchar *interface_name,
                                 const gchar *property_name, GVariant **value,
                                 GError **error, gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)property_name;
        (void)error;
        (void)user_data;

        volume = (gdouble)get_current_volume();

        if (volume >= 1)
                volume = volume / 100;

        *value = g_variant_new_double(volume);
        return TRUE;
}

static gboolean get_position(GDBusConnection *connection, const gchar *sender,
                             const gchar *object_path,
                             const gchar *interface_name,
                             const gchar *property_name, GVariant **value,
                             GError **error, gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)property_name;
        (void)error;
        (void)user_data;

        // Convert elapsed_seconds from milliseconds to microseconds
        gint64 positionMicroseconds = llround(get_elapsed_seconds() * G_USEC_PER_SEC);

        *value = g_variant_new_int64(positionMicroseconds);

        return TRUE;
}

static gboolean get_minimum_rate(GDBusConnection *connection,
                                 const gchar *sender, const gchar *object_path,
                                 const gchar *interface_name,
                                 const gchar *property_name, GVariant **value,
                                 GError **error, gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)property_name;
        (void)error;
        (void)user_data;

        *value = g_variant_new_double(minimum_rate);
        return TRUE;
}

static gboolean get_maximum_rate(GDBusConnection *connection,
                                 const gchar *sender, const gchar *object_path,
                                 const gchar *interface_name,
                                 const gchar *property_name, GVariant **value,
                                 GError **error, gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)property_name;
        (void)error;
        (void)user_data;

        *value = g_variant_new_double(maximum_rate);
        return TRUE;
}

static gboolean get_can_go_next(GDBusConnection *connection,
                                const gchar *sender, const gchar *object_path,
                                const gchar *interface_name,
                                const gchar *property_name, GVariant **value,
                                GError **error, gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)property_name;
        (void)error;
        (void)user_data;

        Node *current = get_current_song();
        PlayList *playlist = get_playlist();

        can_go_next =
            (current == NULL || current->next != NULL) ? TRUE : FALSE;
        can_go_next =
            (is_repeat_list_enabled() && playlist->head != NULL) ? TRUE : can_go_next;

        *value = g_variant_new_boolean(can_go_next);
        return TRUE;
}

static gboolean
get_can_go_previous(GDBusConnection *connection, const gchar *sender,
                    const gchar *object_path, const gchar *interface_name,
                    const gchar *property_name, GVariant **value,
                    GError **error, gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)property_name;
        (void)error;
        (void)user_data;

        Node *current = get_current_song();

        can_go_previous =
            (current == NULL || current->prev != NULL) ? TRUE : FALSE;

        *value = g_variant_new_boolean(can_go_previous);
        return TRUE;
}

static gboolean get_can_play(GDBusConnection *connection, const gchar *sender,
                             const gchar *object_path,
                             const gchar *interface_name,
                             const gchar *property_name, GVariant **value,
                             GError **error, gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)property_name;
        (void)error;
        (void)user_data;

        if (get_current_song() == NULL)
                can_play = FALSE;
        else
                can_play = TRUE;

        *value = g_variant_new_boolean(can_play);
        return TRUE;
}

static gboolean get_can_pause(GDBusConnection *connection, const gchar *sender,
                              const gchar *object_path,
                              const gchar *interface_name,
                              const gchar *property_name, GVariant **value,
                              GError **error, gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)property_name;
        (void)error;
        (void)user_data;

        if (get_current_song() == NULL)
                can_pause = FALSE;
        else
                can_pause = TRUE;

        *value = g_variant_new_boolean(can_pause);
        return TRUE;
}

static gboolean get_can_seek(GDBusConnection *connection, const gchar *sender,
                             const gchar *object_path,
                             const gchar *interface_name,
                             const gchar *property_name, GVariant **value,
                             GError **error, gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)property_name;
        (void)error;
        (void)user_data;

        *value = g_variant_new_boolean(can_seek);
        return TRUE;
}

static gboolean get_can_control(GDBusConnection *connection,
                                const gchar *sender, const gchar *object_path,
                                const gchar *interface_name,
                                const gchar *property_name, GVariant **value,
                                GError **error, gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)property_name;
        (void)error;
        (void)user_data;

        *value = g_variant_new_boolean(can_control);
        return TRUE;
}
#endif

#ifndef __APPLE__
static GVariant *get_property_callback(GDBusConnection *connection,
                                       const gchar *sender,
                                       const gchar *object_path,
                                       const gchar *interface_name,
                                       const gchar *property_name,
                                       GError **error, gpointer user_data)
{

        GVariant *value = NULL;

        if (g_strcmp0(property_name, "PlaybackStatus") == 0) {
                get_playback_status(connection, sender, object_path,
                                    interface_name, property_name, &value,
                                    error, user_data);
        } else if (g_strcmp0(property_name, "LoopStatus") == 0) {
                get_loop_status(connection, sender, object_path, interface_name,
                                property_name, &value, error, user_data);
        } else if (g_strcmp0(property_name, "Rate") == 0) {
                get_rate(connection, sender, object_path, interface_name,
                         property_name, &value, error, user_data);
        } else if (g_strcmp0(property_name, "Shuffle") == 0) {
                get_shuffle(connection, sender, object_path, interface_name,
                            property_name, &value, error, user_data);
        } else if (g_strcmp0(property_name, "Metadata") == 0) {
                get_metadata(connection, sender, object_path, interface_name,
                             property_name, &value, error, user_data);
        } else if (g_strcmp0(property_name, "Volume") == 0) {
                get_volume_mpris(connection, sender, object_path, interface_name,
                                 property_name, &value, error, user_data);
        } else if (g_strcmp0(property_name, "Position") == 0) {
                get_position(connection, sender, object_path, interface_name,
                             property_name, &value, error, user_data);
        } else if (g_strcmp0(property_name, "MinimumRate") == 0) {
                get_minimum_rate(connection, sender, object_path,
                                 interface_name, property_name, &value, error,
                                 user_data);
        } else if (g_strcmp0(property_name, "MaximumRate") == 0) {
                get_maximum_rate(connection, sender, object_path,
                                 interface_name, property_name, &value, error,
                                 user_data);
        } else if (g_strcmp0(property_name, "CanGoNext") == 0) {
                get_can_go_next(connection, sender, object_path, interface_name,
                                property_name, &value, error, user_data);
        } else if (g_strcmp0(property_name, "CanGoPrevious") == 0) {
                get_can_go_previous(connection, sender, object_path,
                                    interface_name, property_name, &value,
                                    error, user_data);
        } else if (g_strcmp0(property_name, "CanPlay") == 0) {
                get_can_play(connection, sender, object_path, interface_name,
                             property_name, &value, error, user_data);
        } else if (g_strcmp0(property_name, "CanPause") == 0) {
                get_can_pause(connection, sender, object_path, interface_name,
                              property_name, &value, error, user_data);
        } else if (g_strcmp0(property_name, "CanSeek") == 0) {
                get_can_seek(connection, sender, object_path, interface_name,
                             property_name, &value, error, user_data);
        } else if (g_strcmp0(property_name, "CanControl") == 0) {
                get_can_control(connection, sender, object_path, interface_name,
                                property_name, &value, error, user_data);
        } else if (g_strcmp0(property_name, "DesktopIconName") == 0) {
                get_desktop_icon_name(connection, sender, object_path,
                                      interface_name, property_name, &value,
                                      error, user_data);
        } else if (g_strcmp0(property_name, "DesktopEntry") == 0) {
                get_desktop_entry(connection, sender, object_path,
                                  interface_name, property_name, &value, error,
                                  user_data);
        } else if (g_strcmp0(property_name, "Identity") == 0) {
                get_identity(connection, sender, object_path, interface_name,
                             property_name, &value, error, user_data);
        } else {
                g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                            "Unknown property");
        }

        // Check if value is NULL and set an error if needed
        if (value == NULL && error == NULL) {
                g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                            "Property value is NULL");
        }

        return value;
}

static gboolean
set_property_callback(GDBusConnection *connection, const gchar *sender,
                      const gchar *object_path, const gchar *interface_name,
                      const gchar *property_name, GVariant *value,
                      GError **error, gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)property_name;
        (void)user_data;

        if (g_strcmp0(interface_name, "org.mpris.MediaPlayer2.Player") == 0) {
                if (g_strcmp0(property_name, "PlaybackStatus") == 0) {
                        g_set_error(
                            error, G_IO_ERROR, G_IO_ERROR_FAILED,
                            "Setting PlaybackStatus property not supported");
                        return FALSE;
                } else if (g_strcmp0(property_name, "Volume") == 0) {
                        double new_volume;
                        g_variant_get(value, "d", &new_volume);

                        if (new_volume > 1.0)
                                new_volume = 1.0;

                        if (new_volume < 0.0)
                                new_volume = 0.0;

                        new_volume *= 100;

                        set_volume((int)new_volume);
                        return TRUE;
                } else if (g_strcmp0(property_name, "LoopStatus") == 0) {
                        toggle_repeat();
                        return TRUE;
                } else if (g_strcmp0(property_name, "Shuffle") == 0) {
                        toggle_shuffle();
                        return TRUE;
                } else if (g_strcmp0(property_name, "Position") == 0) {
                        gint64 new_position;
                        g_variant_get(value, "x", &new_position);

                        return set_position(new_position, get_current_song_duration());
                } else {
                        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                                    "Setting property not supported");
                        return FALSE;
                }
        } else {
                g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                            "Unknown interface");
                return FALSE;
        }
}
#endif

#ifndef __APPLE__
// MPRIS MediaPlayer2 interface vtable
static const GDBusInterfaceVTable media_player_interface_vtable = {
    .method_call = handle_method_call, // We're using individual method handlers
    .get_property =
        get_property_callback, // Handle the property getters individually
    .set_property = set_property_callback,
    .padding = {handle_raise, handle_quit}};

// MPRIS Player interface vtable
static const GDBusInterfaceVTable player_interface_vtable = {
    .method_call = handle_method_call, // We're using individual method handlers
    .get_property =
        get_property_callback, // Handle the property getters individually
    .set_property = set_property_callback,
    .padding = {handle_next, handle_previous, handle_pause, handle_play_pause,
                handle_stop, handle_play, handle_seek, handle_set_position}};
#endif

void emit_playback_stopped_mpris()
{
#ifndef __APPLE__
        if (get_gd_bus_connection()) {
                g_dbus_connection_call(
                    get_gd_bus_connection(), NULL, "/org/mpris/MediaPlayer2",
                    "org.freedesktop.DBus.Properties", "Set",
                    g_variant_new("(ssv)", "org.mpris.MediaPlayer2.Player",
                                  "PlaybackStatus",
                                  g_variant_new_string("Stopped")),
                    G_VARIANT_TYPE("(v)"), G_DBUS_CALL_FLAGS_NONE, -1, NULL,
                    NULL, NULL);
        }
#endif
}

void cleanup_mpris(void)
{
#ifndef __APPLE__
        if (registration_id > 0) {
                g_dbus_connection_unregister_object(get_gd_bus_connection(),
                                                    registration_id);
                registration_id = -1;
        }

        if (player_registration_id > 0) {
                g_dbus_connection_unregister_object(get_gd_bus_connection(),
                                                    player_registration_id);
                player_registration_id = -1;
        }

        if (bus_name_id > 0) {
                g_bus_unown_name(bus_name_id);
                bus_name_id = -1;
        }

        if (get_gd_bus_connection() != NULL) {
                g_object_unref(get_gd_bus_connection());
                set_gd_bus_connection(NULL);
        }

        if (get_g_main_context() != NULL) {
                g_main_context_unref(get_g_main_context());
                set_g_main_context(NULL);
        }
#endif
}

void init_mpris(void)
{
#ifndef __APPLE__
        AppState *state = get_app_state();

        if (get_g_main_context() == NULL) {
                set_g_main_context(g_main_context_new());
        }

        GDBusNodeInfo *introspection_data =
            g_dbus_node_info_new_for_xml(introspection_xml, NULL);
        set_gd_bus_connection(g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL));

        if (!get_gd_bus_connection()) {
                g_dbus_node_info_unref(introspection_data);
                g_printerr("Failed to connect to D-Bus\n");
                exit(0);
        }

        const char *app_name = "org.mpris.MediaPlayer2.kew";

        GError *error = NULL;
        bus_name_id = g_bus_own_name_on_connection(
            get_gd_bus_connection(), app_name, G_BUS_NAME_OWNER_FLAGS_NONE,
            on_bus_name_acquired, on_bus_name_lost, NULL, NULL);

        if (bus_name_id == 0) {
                printf(_("Failed to own D-Bus name: %s\n"), app_name);
                exit(0);
        }

        registration_id = g_dbus_connection_register_object(
            get_gd_bus_connection(), "/org/mpris/MediaPlayer2",
            introspection_data->interfaces[0], &media_player_interface_vtable,
            state, NULL, &error);

        if (!registration_id) {
                g_dbus_node_info_unref(introspection_data);
                g_printerr("Failed to register media player object: %s\n",
                           error->message);
                g_error_free(error);
                exit(0);
        }

        player_registration_id = g_dbus_connection_register_object(
            get_gd_bus_connection(), "/org/mpris/MediaPlayer2",
            introspection_data->interfaces[1], &player_interface_vtable, state,
            NULL, &error);

        if (!player_registration_id) {
                g_dbus_node_info_unref(introspection_data);
                g_printerr("Failed to register media player object: %s\n",
                           error->message);
                g_error_free(error);
                exit(0);
        }

        g_dbus_node_info_unref(introspection_data);
#endif
}

void emit_start_playing_mpris()
{
#ifndef __APPLE__
        GVariant *parameters = g_variant_new("(s)", "Playing");
        g_dbus_connection_emit_signal(
            get_gd_bus_connection(), NULL, "/org/mpris/MediaPlayer2",
            "org.mpris.MediaPlayer2.Player", "PlaybackStatusChanged",
            parameters, NULL);
#endif
}

gchar *sanitize_title(const gchar *title)
{
        gchar *sanitized = g_strdup(title);

        // Replace underscores with hyphens, otherwise some widgets have a
        // problem
        g_strdelimit(sanitized, "_", '-');

        // Duplicate string otherwise widgets have a problem with certain
        // strings for some reason
        gchar *sanitized_dup = g_strdup_printf("%s", sanitized);

        g_free(sanitized);

        return sanitized_dup;
}

#ifndef __APPLE__
static guint64 last_emit_time = 0;
#endif

void emit_properties_changed(GDBusConnection *connection,
                             const gchar *property_name, GVariant *new_value)
{
#ifndef __APPLE__
        GVariantBuilder changed_properties_builder;

        if (connection == NULL || property_name == NULL || new_value == NULL)
                return;

        // Initialize the builder for changed properties
        g_variant_builder_init(&changed_properties_builder,
                               G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_add(&changed_properties_builder, "{sv}",
                              property_name, new_value);

        GError *error = NULL;
        gboolean result = g_dbus_connection_emit_signal(
            connection, NULL, "/org/mpris/MediaPlayer2",
            "org.freedesktop.DBus.Properties", "PropertiesChanged",
            g_variant_new("(sa{sv}as)", "org.mpris.MediaPlayer2.Player",
                          &changed_properties_builder, NULL),
            &error);

        if (!result) {
                g_critical("Failed to emit PropertiesChanged signal: %s",
                           error->message);
                g_error_free(error);
        } else {
                g_debug("PropertiesChanged signal emitted successfully.");
        }

        g_variant_builder_clear(&changed_properties_builder);
#else
        (void)connection;
        (void)property_name;
        (void)new_value;
#endif
}

void emit_volume_changed(void)
{
#ifndef __APPLE__
        gdouble newVolume = (gdouble)get_current_volume() / 100;

        if (newVolume > 1.0)
                return;

        // Emit the PropertiesChanged signal for the volume property
        GVariant *volume_variant = g_variant_new_double(newVolume);
        emit_properties_changed(get_gd_bus_connection(), "volume", volume_variant);
#endif
}

void emit_shuffle_changed(void)
{
#ifndef __APPLE__
        gboolean shuffle_enabled = is_shuffle_enabled();

        // Emit the PropertiesChanged signal for the volume property
        GVariant *volume_variant = g_variant_new_boolean(shuffle_enabled);
        emit_properties_changed(get_gd_bus_connection(), "Shuffle", volume_variant);
#endif
}

void emit_metadata_changed(const gchar *title, const gchar *artist,
                           const gchar *album, const gchar *cover_art_path,
                           const gchar *track_id, Node *current_song, gint64 length)
{
#ifndef __APPLE__
        guint64 current_time = g_get_monotonic_time();
        if (current_time - last_emit_time < 500000) // 0.5 seconds
        {
                g_debug("Debounced signal emission.");
                return;
        }

        last_emit_time = current_time;

        if (!title || !album || !track_id) {
                g_warning(
                    "Invalid metadata: title, album, or track_id is NULL.");
                return;
        }

        gchar *coverArtUrl = NULL;

        gchar *sanitized_title = sanitize_title(title);

        g_debug("Starting to build metadata.");
        GVariantBuilder metadata_builder;
        g_variant_builder_init(&metadata_builder, G_VARIANT_TYPE_DICTIONARY);
        g_variant_builder_add(&metadata_builder, "{sv}", "xesam:title",
                              g_variant_new_string(sanitized_title));
        g_free(sanitized_title);

        const gchar *artist_list[2];
        if (artist) {
                artist_list[0] = artist;
                artist_list[1] = NULL;
        } else {
                artist_list[0] = "";
                artist_list[1] = NULL;
        }
        g_variant_builder_add(&metadata_builder, "{sv}", "xesam:artist",
                              g_variant_new_strv(artist_list, -1));
        g_variant_builder_add(&metadata_builder, "{sv}", "xesam:album",
                              g_variant_new_string(album));

        if (cover_art_path && *cover_art_path != '\0') {
                coverArtUrl = g_strdup_printf("file://%s", cover_art_path);
                g_variant_builder_add(&metadata_builder, "{sv}", "mpris:artUrl",
                                      g_variant_new_string(coverArtUrl));
                g_debug("Cover art URL added: %s", coverArtUrl);
                g_free(coverArtUrl);
        }

        g_variant_builder_add(&metadata_builder, "{sv}", "mpris:trackid",
                              g_variant_new_object_path(track_id));
        g_variant_builder_add(&metadata_builder, "{sv}", "mpris:length",
                              g_variant_new_int64(length));

        GVariant *metadata_variant = g_variant_builder_end(&metadata_builder);

        if (!metadata_variant) {
                g_warning("Failed to end metadata GVariantBuilder.");
                return;
        }

        g_debug("Metadata built successfully.");

        GVariantBuilder changed_properties_builder;
        g_variant_builder_init(&changed_properties_builder,
                               G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_add(&changed_properties_builder, "{sv}", "Metadata",
                              metadata_variant);
        g_variant_builder_add(
            &changed_properties_builder, "{sv}", "CanGoPrevious",
            g_variant_new_boolean(
                (current_song != NULL && current_song->prev != NULL)));

        PlayList *playlist = get_playlist();

        can_go_next =
            (current_song == NULL || current_song->next != NULL) ? TRUE : FALSE;
        can_go_next =
            (is_repeat_list_enabled() && playlist->head != NULL) ? TRUE : can_go_next;

        g_variant_builder_add(&changed_properties_builder, "{sv}", "CanGoNext",
                              g_variant_new_boolean(can_go_next));
        g_variant_builder_add(&changed_properties_builder, "{sv}", "Shuffle",
                              g_variant_new_boolean(is_shuffle_enabled()));
        g_variant_builder_add(
            &changed_properties_builder, "{sv}", "CanPlay",
            g_variant_new_boolean(length != 0 ? true : false));
        g_variant_builder_add(
            &changed_properties_builder, "{sv}", "CanPause",
            g_variant_new_boolean(length != 0 ? true : false));

        if (is_repeat_enabled())
                g_variant_builder_add(&changed_properties_builder, "{sv}",
                                      "LoopStatus",
                                      g_variant_new_string("Track"));
        else if (is_repeat_list_enabled())
                g_variant_builder_add(&changed_properties_builder, "{sv}",
                                      "LoopStatus",
                                      g_variant_new_string("List"));
        else
                g_variant_builder_add(&changed_properties_builder, "{sv}",
                                      "LoopStatus",
                                      g_variant_new_string("None"));

        can_seek = true;

        g_variant_builder_add(&changed_properties_builder, "{sv}", "CanSeek",
                              g_variant_new_boolean(can_seek));

        g_debug("PropertiesChanged signal is ready to be emitted.");

        GError *error = NULL;
        gboolean result = g_dbus_connection_emit_signal(
            get_gd_bus_connection(), NULL, "/org/mpris/MediaPlayer2",
            "org.freedesktop.DBus.Properties", "PropertiesChanged",
            g_variant_new("(sa{sv}as)", "org.mpris.MediaPlayer2.Player",
                          &changed_properties_builder, NULL),
            &error);

        if (!result) {
                g_critical("Failed to emit PropertiesChanged signal: %s",
                           error->message);
                g_error_free(error);
        } else {
                g_debug("PropertiesChanged signal emitted successfully.");
        }

        g_variant_builder_clear(&changed_properties_builder);
        g_variant_builder_clear(&metadata_builder);
#else
        (void)title;
        (void)artist;
        (void)album;
        (void)cover_art_path;
        (void)track_id;
        (void)current_song;
        (void)length;
#endif
}
