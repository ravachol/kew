/**
 * @file mpris.c
 * @brief MPRIS (Media Player Remote Interfacing Specification) integration.
 *
 * Implements D-Bus MPRIS interface support for desktop integration,
 * allowing external clients to control playback (e.g., GNOME, KDE, etc.).
 */

#include "mpris.h"

#include "common/appstate.h"
#include "common/common.h"
#include "common/events.h"
#include "common/model.h"

#include "sys_integration.h"

#include "ui/control_ui.h"
#include "ui/components.h"
#include "ui/input.h"

#include "update/messages.h"

#include "utils/k_log.h"

#include "ops/playback_clock.h"
#include "ops/playback_state.h"
#include "ops/playlist_ops.h"

#ifdef USE_MACOS_MEDIA
#include "macos_nowplaying.h"
#endif

#ifdef USE_SMTC
#include "smtc.h"
#endif

#include <glib.h>
#include <math.h>

#ifdef USE_DBUS

static guint registration_id;
static guint bus_name_id;
static guint player_registration_id;
static guint track_list_registration_id;
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
    "  <interface name=\"org.mpris.MediaPlayer2.TrackList\">\n"
    "    <method name=\"GetTracksMetadata\">\n"
    "      <arg name=\"TrackIds\" type=\"ao\" direction=\"in\"/>\n"
    "      <arg name=\"Metadata\" type=\"aa{sv}\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"AddTrack\">\n"
    "      <arg name=\"Uri\" type=\"s\" direction=\"in\"/>\n"
    "      <arg name=\"AfterTrack\" type=\"o\" direction=\"in\"/>\n"
    "      <arg name=\"SetAsCurrent\" type=\"b\" direction=\"in\"/>\n"
    "    </method>\n"
    "    <method name=\"RemoveTrack\">\n"
    "      <arg name=\"TrackId\" type=\"o\" direction=\"in\"/>\n"
    "    </method>\n"
    "    <method name=\"GoTo\">\n"
    "      <arg name=\"TrackId\" type=\"o\" direction=\"in\"/>\n"
    "    </method>\n"
    "    <signal name=\"TrackListReplaced\">\n"
    "      <arg name=\"Tracks\" type=\"ao\"/>\n"
    "      <arg name=\"CurrentTrack\" type=\"o\"/>\n"
    "    </signal>\n"
    "    <signal name=\"TrackAdded\">\n"
    "      <arg name=\"Metadata\" type=\"a{sv}\"/>\n"
    "      <arg name=\"AfterTrack\" type=\"o\"/>\n"
    "    </signal>\n"
    "    <signal name=\"TrackRemoved\">\n"
    "      <arg name=\"TrackId\" type=\"o\"/>\n"
    "    </signal>\n"
    "    <signal name=\"TrackMetadataChanged\">\n"
    "      <arg name=\"TrackId\" type=\"o\"/>\n"
    "      <arg name=\"Metadata\" type=\"a{sv}\"/>\n"
    "    </signal>\n"
    "    <property name=\"Tracks\" type=\"ao\" access=\"read\"/>\n"
    "    <property name=\"CanEditTracks\" type=\"b\" access=\"read\"/>\n"
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

        dispatch_msg((struct Msg){.type = MSG_QUIT});
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

static gboolean get_has_track_list(GDBusConnection *connection,
                                   const gchar *sender,
                                   const gchar *object_path,
                                   const gchar *interface_name,
                                   const gchar *property_name,
                                   GVariant **value,
                                   GError **error,
                                   gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)property_name;
        (void)error;
        (void)user_data;

        *value = g_variant_new_boolean(track_list_registration_id != 0);
        return TRUE;
}

static gboolean get_can_raise(GDBusConnection *connection,
                                   const gchar *sender,
                                   const gchar *object_path,
                                   const gchar *interface_name,
                                   const gchar *property_name,
                                   GVariant **value,
                                   GError **error,
                                   gpointer user_data)
{
        (void)connection;
        (void)sender;
        (void)object_path;
        (void)interface_name;
        (void)property_name;
        (void)error;
        (void)user_data;

        *value = g_variant_new_boolean(FALSE);
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

        dispatch_msg((struct Msg){.type = MSG_NEXT});
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

        dispatch_msg((struct Msg){.type = MSG_PREV});
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

        dispatch_msg((struct Msg){.type = MSG_PAUSE});
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

        dispatch_msg((struct Msg){.type = MSG_PLAY_PAUSE});

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

        if (!is_stopped())
                dispatch_msg((struct Msg){.type = MSG_STOP});

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

        if (get_current_song() == NULL)
                dispatch_msg((struct Msg){.type = MSG_ENQUEUEANDPLAY});
        else
                dispatch_msg((struct Msg){.type = MSG_PLAY});

        g_dbus_method_invocation_return_value(invocation, NULL);
}

static void emit_seeked(gint64 position)
{
#ifdef USE_DBUS
        GDBusConnection *connection = get_gd_bus_connection();

        if (connection == NULL)
                return;

        GError *error = NULL;

        gboolean result = g_dbus_connection_emit_signal(
            connection,
            NULL,
            "/org/mpris/MediaPlayer2",
            "org.mpris.MediaPlayer2.Player",
            "Seeked",
            g_variant_new("(x)", position),
            &error);

        if (!result) {
                g_warning("Failed to emit Seeked signal: %s",
                          error ? error->message : "unknown error");

                if (error)
                        g_error_free(error);
        }
#else
        (void)position;
#endif
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
        gboolean success = false;
        g_variant_get(parameters, "(x)", &offset);

        Model *model = get_model();
        success = seek_position(offset, model->song_duration);

        if (success) {
                gint64 new_position =
                    llround(get_elapsed_seconds() * G_USEC_PER_SEC);

                emit_seeked(new_position);

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
        gboolean success = false;

        // - "o" is an object path (or track identifier)
        // - "x" is a 64-bit integer representing the position
        g_variant_get(parameters, "(&ox)", &track_id, &new_position);

        success = set_position(new_position);

        if (success) {
                // If setting the position was successful, return success with
                // no additional value
                gint64 actual_position =
                    llround(get_elapsed_seconds() * G_USEC_PER_SEC);

                emit_seeked(actual_position);

                g_dbus_method_invocation_return_value(invocation, NULL);
        } else {
                // If setting the position failed, return an error
                g_dbus_method_invocation_return_error(
                    invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                    "Failed to set position for track %s", track_id);
        }
}
#endif

#ifdef USE_DBUS
typedef struct {
        gchar *track_id;
        gchar *file_path;
        double duration;
} TrackSnapshot;

static void free_track_snapshot(gpointer data)
{
        TrackSnapshot *track = data;
        g_free(track->track_id);
        g_free(track->file_path);
        g_free(track);
}

static void track_path(const Node *node, char *path, size_t size)
{
        g_snprintf(path, size, "/org/kew/Track/t%" G_GUINT64_FORMAT,
                   (guint64)node->tracklist_id);
}

static Node *find_track_locked(PlayList *playlist, const char *path)
{
        char node_path[96];
        for (Node *node = playlist->head; node != NULL; node = node->next) {
                track_path(node, node_path, sizeof(node_path));
                if (g_strcmp0(path, node_path) == 0)
                        return node;
        }
        return NULL;
}

static GVariant *track_metadata(const TrackSnapshot *track)
{
        GVariantBuilder metadata;
        g_variant_builder_init(&metadata, G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_add(&metadata, "{sv}", "mpris:trackid",
                              g_variant_new_object_path(track->track_id));

        char title[KEW_NAME_MAX];
        Node title_node = {.song.file_path = track->file_path};
        prepare_playlist_string(&title_node, title, sizeof(title));
        gchar *valid_title = g_utf8_make_valid(title, -1);
        g_variant_builder_add(&metadata, "{sv}", "xesam:title",
                              g_variant_new_string(valid_title));
        g_free(valid_title);

        if (track->file_path && g_path_is_absolute(track->file_path)) {
                gchar *uri = g_filename_to_uri(track->file_path, NULL, NULL);
                if (uri) {
                        g_variant_builder_add(&metadata, "{sv}", "xesam:url",
                                              g_variant_new_string(uri));
                        g_free(uri);
                }
        }

        if (isfinite(track->duration) && track->duration > 0 &&
            track->duration < (double)G_MAXINT64 / G_USEC_PER_SEC) {
                g_variant_builder_add(&metadata, "{sv}", "mpris:length",
                                      g_variant_new_int64(llround(track->duration * G_USEC_PER_SEC)));
        }

        return g_variant_builder_end(&metadata);
}

static void handle_track_list_method(GVariant *parameters,
                                     const gchar *method_name,
                                     GDBusMethodInvocation *invocation)
{
        if (g_strcmp0(method_name, "AddTrack") == 0 ||
            g_strcmp0(method_name, "RemoveTrack") == 0) {
                g_dbus_method_invocation_return_dbus_error(
                    invocation, "org.mpris.MediaPlayer2.TrackList.Error.NotSupported",
                    "TrackList editing is not supported");
                return;
        }

        PlayList *playlist = get_playlist();

        if (g_strcmp0(method_name, "GetTracksMetadata") != 0) {
                g_dbus_method_invocation_return_dbus_error(
                    invocation, "org.freedesktop.DBus.Error.UnknownMethod",
                    "No such method");
                return;
        }

        GVariantIter *paths;
        const gchar *path;
        g_variant_get(parameters, "(ao)", &paths);
        GPtrArray *tracks = g_ptr_array_new_with_free_func(free_track_snapshot);
        gboolean unknown = FALSE;

        pthread_mutex_lock(&playlist->mutex);
        while (g_variant_iter_loop(paths, "&o", &path)) {
                Node *node = find_track_locked(playlist, path);
                if (!node) {
                        unknown = TRUE;
                        break;
                }
                TrackSnapshot *track = g_new0(TrackSnapshot, 1);
                track->track_id = g_strdup(path);
                track->file_path = g_strdup(node->song.file_path);
                track->duration = node->song.duration;
                g_ptr_array_add(tracks, track);
        }
        pthread_mutex_unlock(&playlist->mutex);
        g_variant_iter_free(paths);

        if (unknown) {
                g_ptr_array_unref(tracks);
                g_dbus_method_invocation_return_dbus_error(
                    invocation, "org.freedesktop.DBus.Error.InvalidArgs",
                    "Unknown TrackList track ID");
                return;
        }

        GVariantBuilder metadata_list;
        g_variant_builder_init(&metadata_list, G_VARIANT_TYPE("aa{sv}"));
        for (guint i = 0; i < tracks->len; i++)
                g_variant_builder_add_value(&metadata_list,
                                            track_metadata(g_ptr_array_index(tracks, i)));
        g_ptr_array_unref(tracks);
        g_dbus_method_invocation_return_value(
            invocation, g_variant_new("(aa{sv})", &metadata_list));
}

static void handle_method_call(GDBusConnection *connection, const gchar *sender,
                               const gchar *object_path,
                               const gchar *interface_name,
                               const gchar *method_name, GVariant *parameters,
                               GDBusMethodInvocation *invocation,
                               gpointer user_data)
{
        if (g_strcmp0(interface_name, "org.mpris.MediaPlayer2.TrackList") == 0) {
                handle_track_list_method(parameters, method_name, invocation);
        } else if (g_strcmp0(method_name, "PlayPause") == 0) {
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

#ifdef USE_DBUS
static void on_bus_name_acquired(GDBusConnection *connection, const gchar *name,
                                 gpointer user_data)
{
        (void)connection;
        (void)name;
        (void)user_data;
        k_log("DBUS name aquired");
}

static void on_bus_name_lost(GDBusConnection *connection, const gchar *name,
                             gpointer user_data)
{
        (void)connection;
        (void)name;
        (void)user_data;
        k_log("DBUS name lost");
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

        if (is_paused()) {
                status = "Paused";
        } else if (get_current_song() == NULL || is_stopped()) {
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

        Model *model = get_model();

        switch (model->state.settings.repeatState) {
        case SOUND_STATE_REPEAT_OFF:
                *value = g_variant_new_string("None");
                break;
        case SOUND_STATE_REPEAT:
                *value = g_variant_new_string("Track");
                break;
        case SOUND_STATE_REPEAT_LIST:
                *value = g_variant_new_string("Playlist");
                break;
        default:
                *value = g_variant_new_string("None");
                break;
        }

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

// Convert a filesystem cover path to a properly escaped file:// URI for
// mpris:artUrl
static gchar *path_to_uri(const char *path)
{
        if (!path || path[0] == '\0')
                return NULL;

        GError *error = NULL;
        gchar *uri = g_filename_to_uri(path, NULL, &error);
        if (uri)
                return uri;

        if (error) {
                g_debug("path_to_uri: %s", error->message);
                g_error_free(error);
        }

        // Fallback for absolute Unix paths if g_filename_to_uri fails
        if (path[0] == '/')
                return g_strdup_printf("file://%s", path);

        return NULL;
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

        Model *model = get_model();
        int mutex_result = pthread_mutex_lock(&(model->playbackState.switch_mutex));

        if (mutex_result != 0) {
                k_log("get_metadata: Failed to lock switch mutex.\n");
                return FALSE;
        }

        SongData *current_song_data = model->songdata;
        Node *current_song = get_current_song();

        GVariantBuilder metadata_builder;
        g_variant_builder_init(&metadata_builder, G_VARIANT_TYPE_DICTIONARY);

        if (current_song != NULL && current_song_data != NULL &&
            current_song_data->metadata != NULL) {
                g_variant_builder_add(
                    &metadata_builder, "{sv}", "xesam:title",
                    g_variant_new_string(current_song_data->metadata->title));

                // Build list of strings for artist
                const gchar *artist_list_storage[2];
                artist_list_storage[0] = (g_strcmp0(current_song_data->metadata->artist, "") == 0) ? "" : current_song_data->metadata->artist;
                artist_list_storage[1] = NULL;

                g_variant_builder_add(&metadata_builder, "{sv}", "xesam:artist",
                                      g_variant_new_strv(artist_list_storage, -1));

                g_variant_builder_add(
                    &metadata_builder, "{sv}", "xesam:album",
                    g_variant_new_string(current_song_data->metadata->album));
                g_variant_builder_add(
                    &metadata_builder, "{sv}", "xesam:contentCreated",
                    g_variant_new_string(current_song_data->metadata->date));

                gchar *coverArtUrl =
                    path_to_uri(current_song_data->cover_art_path);
                if (coverArtUrl) {
                        g_variant_builder_add(&metadata_builder, "{sv}",
                                              "mpris:artUrl",
                                              g_variant_new_string(coverArtUrl));
                        g_free(coverArtUrl);
                } else {
                        g_variant_builder_add(&metadata_builder, "{sv}",
                                              "mpris:artUrl",
                                              g_variant_new_string(""));
                }

                gchar *track_uri = path_to_uri(current_song_data->file_path);

                if (track_uri) {
                        g_variant_builder_add(
                            &metadata_builder,
                            "{sv}",
                            "xesam:url",
                            g_variant_new_string(track_uri));

                        g_free(track_uri);
                }

                char current_path[96];
                track_path(current_song, current_path, sizeof(current_path));
                g_variant_builder_add(
                    &metadata_builder, "{sv}", "mpris:trackid",
                    g_variant_new_object_path(current_path));

                gint64 length =
                    llround(current_song_data->duration * G_USEC_PER_SEC);
                g_variant_builder_add(&metadata_builder, "{sv}", "mpris:length",
                                      g_variant_new_int64(length));
        } else {
                g_variant_builder_add(&metadata_builder, "{sv}", "xesam:title",
                                      g_variant_new_string(""));

                static const gchar *empty_artist_list[] = {"", NULL};
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
                g_variant_builder_add(&metadata_builder, "{sv}", "xesam:url",
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

        pthread_mutex_unlock(&(model->playbackState.switch_mutex));

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

        volume = (gdouble)get_volume();

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
            (current != NULL && current->prev != NULL);

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

        Model *model = get_model();

        if (get_current_song() == NULL && model->playlist->count == 0)
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

#ifdef USE_DBUS
static GVariant *get_property_callback(GDBusConnection *connection,
                                       const gchar *sender,
                                       const gchar *object_path,
                                       const gchar *interface_name,
                                       const gchar *property_name,
                                       GError **error, gpointer user_data)
{

        GVariant *value = NULL;

        if (g_strcmp0(interface_name, "org.mpris.MediaPlayer2.TrackList") == 0) {
                if (g_strcmp0(property_name, "CanEditTracks") == 0)
                        return g_variant_new_boolean(FALSE);
                if (g_strcmp0(property_name, "Tracks") == 0) {
                        PlayList *playlist = get_playlist();
                        GPtrArray *paths = g_ptr_array_new_with_free_func(g_free);
                        pthread_mutex_lock(&playlist->mutex);
                        for (Node *node = playlist->head; node; node = node->next) {
                                char path[96];
                                track_path(node, path, sizeof(path));
                                g_ptr_array_add(paths, g_strdup(path));
                        }
                        pthread_mutex_unlock(&playlist->mutex);

                        GVariantBuilder builder;
                        g_variant_builder_init(&builder, G_VARIANT_TYPE("ao"));
                        for (guint i = 0; i < paths->len; i++)
                                g_variant_builder_add(&builder, "o", (char *)g_ptr_array_index(paths, i));
                        g_ptr_array_unref(paths);
                        return g_variant_builder_end(&builder);
                }
        }

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
        } else if (g_strcmp0(property_name, "HasTrackList") == 0) {
                get_has_track_list(connection, sender, object_path,
                                   interface_name, property_name, &value,
                                   error, user_data);
        } else if (g_strcmp0(property_name, "CanRaise") == 0) {
                get_can_raise(connection, sender, object_path, interface_name,
                                property_name, &value, error, user_data);
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
        if (value == NULL) {
                if (error != NULL && *error == NULL) {
                        g_set_error(
                            error,
                            G_IO_ERROR,
                            G_IO_ERROR_FAILED,
                            "Property value is NULL");
                }

                return NULL;
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

                        if (new_volume > 1.2)
                                new_volume = 1.2;

                        if (new_volume < 0.0)
                                new_volume = 0.0;

                        new_volume *= 100;

                        set_volume((int)new_volume);
                        set_dirty(DIRTY_VISUALIZER);
                        return TRUE;
                } else if (g_strcmp0(property_name, "LoopStatus") == 0) {
                        const gchar *loop_status;

                        g_variant_get(value, "&s", &loop_status);

                        if (g_strcmp0(loop_status, "None") == 0) {
                                set_repeat_state(SOUND_STATE_REPEAT_OFF);
                        } else if (g_strcmp0(loop_status, "Track") == 0) {
                                set_repeat_state(SOUND_STATE_REPEAT);
                        } else if (g_strcmp0(loop_status, "Playlist") == 0) {
                                set_repeat_state(SOUND_STATE_REPEAT_LIST);
                        } else {
                                g_set_error(
                                    error,
                                    G_IO_ERROR,
                                    G_IO_ERROR_INVALID_ARGUMENT,
                                    "Invalid LoopStatus: %s",
                                    loop_status);
                                return FALSE;
                        }
                        return TRUE;
                } else if (g_strcmp0(property_name, "Shuffle") == 0) {
                        toggle_shuffle(get_model());
                        return TRUE;
                } else if (g_strcmp0(property_name, "Position") == 0) {
                        gint64 new_position;
                        g_variant_get(value, "x", &new_position);

                        bool result = false;

                        result = set_position(new_position);

                        return result;

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

#ifdef USE_DBUS
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

static const GDBusInterfaceVTable track_list_interface_vtable = {
    .method_call = handle_method_call,
    .get_property = get_property_callback,
    .set_property = set_property_callback};
#endif

void emit_playback_playing()
{
#ifdef USE_DBUS
        emit_string_property_changed("PlaybackStatus", "Playing");
#elif defined(USE_MACOS_MEDIA)
        macos_set_playback_state_playing();
#elif defined(USE_SMTC)
        smtc_set_playback_playing();
#endif
}

void emit_playback_stopped()
{
#ifdef USE_DBUS
        emit_string_property_changed("PlaybackStatus", "Stopped");
#elif defined(USE_MACOS_MEDIA)
        macos_set_playback_state_stopped();
#elif defined(USE_SMTC)
        smtc_set_playback_stopped();
#endif
}

void emit_playback_paused()
{
#ifdef USE_DBUS
        emit_string_property_changed("PlaybackStatus", "Paused");
#elif defined(USE_MACOS_MEDIA)
        macos_set_playback_state_paused();
#elif defined(USE_SMTC)
        smtc_set_playback_paused();
#endif
}

void mpris_shutdown(void)
{
#ifdef USE_DBUS
        emit_playback_stopped();

        if (registration_id != 0) {
                g_dbus_connection_unregister_object(get_gd_bus_connection(), registration_id);
                registration_id = 0;
        }

        if (player_registration_id != 0) {
                g_dbus_connection_unregister_object(get_gd_bus_connection(), player_registration_id);
                player_registration_id = 0;
        }

        if (track_list_registration_id != 0) {
                g_dbus_connection_unregister_object(get_gd_bus_connection(),
                                                    track_list_registration_id);
                track_list_registration_id = 0;
        }
        if (bus_name_id != 0) {
                g_bus_unown_name(bus_name_id);
                bus_name_id = 0;
        }

        if (get_gd_bus_connection() != NULL) {
                g_object_unref(get_gd_bus_connection());
                set_gd_bus_connection(NULL);
        }

        if (get_g_main_context() != NULL) {
                g_main_context_unref(get_g_main_context());
                set_g_main_context(NULL);
        }
#elif defined(USE_MACOS_MEDIA)

        cleanup_macos_nowplaying();

#elif defined(USE_SMTC)

        smtc_shutdown();

#endif
}

void mpris_init(void)
{
        k_log("mpris_init entered");
#ifdef USE_DBUS
        AppState *state = get_app_state();

        if (get_g_main_context() == NULL) {
                set_g_main_context(g_main_context_new());
        }

        GDBusNodeInfo *introspection_data =
            g_dbus_node_info_new_for_xml(introspection_xml, NULL);
        GError *error = NULL;

        GDBusConnection *conn =
            g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);

        if (!conn) {
                g_printerr("%s\n", error->message);
                k_log("%s", error->message);
                k_log("Failed to connect to D-Bus. Either 1) start D-BUS, 2) recompile with USE_DBUS=0 or 3) use dbus-launch kew 4) run: doas setcap -r /usr/local/bin/kew");
                set_error_message(error->message);
                g_error_free(error);
                g_dbus_node_info_unref(introspection_data);
                return;
        }

        set_gd_bus_connection(conn);

        if (!get_gd_bus_connection()) {
                g_dbus_node_info_unref(introspection_data);
                k_log("Failed to connect to D-Bus. Either 1) start D-BUS, 2) recompile with USE_DBUS=0 or 3) use dbus-launch kew");
                set_error_message("Failed to connect to D-Bus. Either 1) start D-BUS, 2) recompile with USE_DBUS=0 or 3) use dbus-launch kew 4) run: doas setcap -r /usr/local/bin/kew");
                return;
        }

        const char *app_name = "org.mpris.MediaPlayer2.kew";

        bus_name_id = g_bus_own_name_on_connection(
            conn, app_name, G_BUS_NAME_OWNER_FLAGS_NONE,
            on_bus_name_acquired, on_bus_name_lost, NULL, NULL);

        if (bus_name_id == 0) {
                printf(_("Failed to own D-Bus name: %s\n"), app_name);
                return;
        }

        registration_id = g_dbus_connection_register_object(
            get_gd_bus_connection(), "/org/mpris/MediaPlayer2",
            introspection_data->interfaces[0], &media_player_interface_vtable,
            state, NULL, &error);

        if (!registration_id) {
                g_dbus_node_info_unref(introspection_data);
                g_printerr("Failed to register mpris");
                g_error_free(error);
                return;
        }

        player_registration_id = g_dbus_connection_register_object(
            get_gd_bus_connection(), "/org/mpris/MediaPlayer2",
            introspection_data->interfaces[1], &player_interface_vtable, state,
            NULL, &error);

        if (!player_registration_id) {
                g_dbus_node_info_unref(introspection_data);
                g_printerr("Failed to register mpris");
                g_error_free(error);
                return;
        }

        track_list_registration_id = g_dbus_connection_register_object(
            get_gd_bus_connection(), "/org/mpris/MediaPlayer2",
            introspection_data->interfaces[2], &track_list_interface_vtable,
            state, NULL, &error);

        if (!track_list_registration_id) {
                g_dbus_node_info_unref(introspection_data);
                g_printerr("Failed to register MPRIS TrackList: %s\n", error->message);
                g_error_free(error);
                return;
        }

        g_dbus_node_info_unref(introspection_data);
#elif defined(USE_MACOS_MEDIA)

        init_macos_nowplaying();

#elif defined(USE_SMTC)

        smtc_init();

#endif

        k_log("mpris_init done");
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

#ifdef USE_DBUS
static guint64 last_emit_time = 0;
#endif

void emit_properties_changed(GDBusConnection *connection,
                             const gchar *property_name, GVariant *new_value)
{
#ifdef USE_DBUS
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
#ifdef USE_DBUS
        gdouble newVolume = (gdouble)get_volume() / 100;

        if (newVolume > 1.0)
                return;

        // Emit the PropertiesChanged signal for the volume property
        GVariant *volume_variant = g_variant_new_double(newVolume);
        emit_properties_changed(get_gd_bus_connection(), "Volume", volume_variant);
#endif
}

void emit_shuffle_changed(void)
{
#ifdef USE_DBUS
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
#ifdef USE_DBUS
        GDBusConnection *connection = get_gd_bus_connection();

        if (connection == NULL)
                return;

        guint64 current_time = g_get_monotonic_time();
        if (current_time - last_emit_time < 500000) // 0.5 seconds
        {
                g_debug("Debounced signal emission.");
                return;
        }

        last_emit_time = current_time;

        char current_path[96];
        if (current_song) {
                track_path(current_song, current_path, sizeof(current_path));
                track_id = current_path;
        }

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

        coverArtUrl = path_to_uri(cover_art_path);
        if (coverArtUrl) {
                g_variant_builder_add(&metadata_builder, "{sv}", "mpris:artUrl",
                                      g_variant_new_string(coverArtUrl));
                g_debug("Cover art URL added: %s", coverArtUrl);
                g_free(coverArtUrl);
        }

        if (current_song) {
                gchar *track_uri = path_to_uri(current_song->song.file_path);

                if (track_uri) {
                        g_variant_builder_add(
                            &metadata_builder,
                            "{sv}",
                            "xesam:url",
                            g_variant_new_string(track_uri));

                        g_free(track_uri);
                }
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

        gboolean can_go_next = FALSE;

        if (current_song != NULL && current_song->next != NULL) {
                can_go_next = TRUE;
        } else if (is_repeat_list_enabled() &&
                   playlist != NULL &&
                   playlist->head != NULL) {
                can_go_next = TRUE;
        }

        g_variant_builder_add(&changed_properties_builder, "{sv}", "CanGoNext",
                              g_variant_new_boolean(can_go_next));
        g_variant_builder_add(&changed_properties_builder, "{sv}", "Shuffle",
                              g_variant_new_boolean(is_shuffle_enabled()));

        g_variant_builder_add(
            &changed_properties_builder, "{sv}", "CanPlay",
            g_variant_new_boolean(current_song != NULL || playlist->count > 0));
        g_variant_builder_add(
            &changed_properties_builder, "{sv}", "CanPause",
            g_variant_new_boolean(current_song != NULL));

        if (is_repeat_enabled())
                g_variant_builder_add(&changed_properties_builder, "{sv}",
                                      "LoopStatus",
                                      g_variant_new_string("Track"));
        else if (is_repeat_list_enabled())
                g_variant_builder_add(&changed_properties_builder, "{sv}",
                                      "LoopStatus",
                                      g_variant_new_string("Playlist"));
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
        g_variant_builder_clear(&metadata_builder);
#elif defined(USE_MACOS_MEDIA)
        (void)track_id;
        (void)current_song;
        macos_set_now_playing_info(title, artist, album, cover_art_path,
                                   (double)length / G_USEC_PER_SEC);
#elif defined(USE_SMTC)
        (void)track_id;
        (void)current_song;
        smtc_update_metadata(title,
                             artist,
                             album,
                             cover_art_path,
                             (double)length / G_USEC_PER_SEC);

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
