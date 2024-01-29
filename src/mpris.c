#include "mpris.h"
/*

mpris.c

Functions related to mpris implementation.

*/

GMainContext *global_main_context = NULL;
GMainLoop *main_loop;
guint registration_id;
guint player_registration_id;
guint bus_name_id;

static const gchar *LoopStatus = "None";
static gdouble Rate = 1.0;
static gdouble Volume = 0.5;
static gdouble MinimumRate = 1.0;
static gdouble MaximumRate = 1.0;
static gboolean CanGoNext = TRUE;
static gboolean CanGoPrevious = TRUE;
static gboolean CanPlay = TRUE;
static gboolean CanPause = TRUE;
static gboolean CanSeek = FALSE;
static gboolean CanControl = TRUE;

void updatePlaybackStatus(const gchar *status)
{
        GVariant *status_variant = g_variant_new_string(status);
        g_dbus_connection_emit_signal(connection, NULL, "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", "PlaybackStatus", g_variant_new("(s)", status_variant), NULL);
        g_variant_unref(status_variant);
}

const gchar *introspection_xml =
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\" \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
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
static const gchar *desktopIconName = ""; // Without file extension
static const gchar *desktopEntry = "";    // The name of your .desktop file

static void handle_raise(GDBusConnection *connection, const gchar *sender,
                         const gchar *object_path, const gchar *interface_name,
                         const gchar *method_name, GVariant *parameters,
                         GDBusMethodInvocation *invocation, gpointer user_data)
{
}

static void handle_quit(GDBusConnection *connection, const gchar *sender,
                        const gchar *object_path, const gchar *interface_name,
                        const gchar *method_name, GVariant *parameters,
                        GDBusMethodInvocation *invocation, gpointer user_data)
{
        quit();
}

static gboolean get_identity(GDBusConnection *connection, const gchar *sender,
                             const gchar *object_path, const gchar *interface_name,
                             const gchar *property_name, GVariant **value,
                             GError **error, gpointer user_data)
{
        *value = g_variant_new_string(identity);
        return TRUE;
}

static gboolean get_desktop_entry(GDBusConnection *connection, const gchar *sender,
                                  const gchar *object_path, const gchar *interface_name,
                                  const gchar *property_name, GVariant **value,
                                  GError **error, gpointer user_data)
{
        *value = g_variant_new_string(desktopEntry);
        return TRUE;
}

static gboolean get_desktop_icon_name(GDBusConnection *connection, const gchar *sender,
                                      const gchar *object_path, const gchar *interface_name,
                                      const gchar *property_name, GVariant **value,
                                      GError **error, gpointer user_data)
{
        *value = g_variant_new_string(desktopIconName);
        return TRUE;
}

static void handle_next(GDBusConnection *connection, const gchar *sender,
                        const gchar *object_path, const gchar *interface_name,
                        const gchar *method_name, GVariant *parameters,
                        GDBusMethodInvocation *invocation, gpointer user_data)
{
        skipToNextSong();
        g_dbus_method_invocation_return_value(invocation, NULL);
}

static void handle_previous(GDBusConnection *connection, const gchar *sender,
                            const gchar *object_path, const gchar *interface_name,
                            const gchar *method_name, GVariant *parameters,
                            GDBusMethodInvocation *invocation, gpointer user_data)
{
        skipToPrevSong();
        g_dbus_method_invocation_return_value(invocation, NULL);
}

static void handle_pause(GDBusConnection *connection, const gchar *sender,
                         const gchar *object_path, const gchar *interface_name,
                         const gchar *method_name, GVariant *parameters,
                         GDBusMethodInvocation *invocation, gpointer user_data)
{
        playbackPause(&totalPauseSeconds, &pauseSeconds, &pause_time);
}

static void handle_play_pause(GDBusConnection *connection, const gchar *sender,
                              const gchar *object_path, const gchar *interface_name,
                              const gchar *method_name, GVariant *parameters,
                              GDBusMethodInvocation *invocation, gpointer user_data)
{
        togglePause(&totalPauseSeconds, &pauseSeconds, &pause_time);
        g_dbus_method_invocation_return_value(invocation, NULL);
}

static void handle_stop(GDBusConnection *connection, const gchar *sender,
                        const gchar *object_path, const gchar *interface_name,
                        const gchar *method_name, GVariant *parameters,
                        GDBusMethodInvocation *invocation, gpointer user_data)
{
        quit();
        g_dbus_method_invocation_return_value(invocation, NULL);
}

static void handle_play(GDBusConnection *connection, const gchar *sender,
                        const gchar *object_path, const gchar *interface_name,
                        const gchar *method_name, GVariant *parameters,
                        GDBusMethodInvocation *invocation, gpointer user_data)
{
        playbackPlay(&totalPauseSeconds, &pauseSeconds, &pause_time);
}

static void handle_seek(GDBusConnection *connection, const gchar *sender,
                        const gchar *object_path, const gchar *interface_name,
                        const gchar *method_name, GVariant *parameters,
                        GDBusMethodInvocation *invocation, gpointer user_data)
{
}

static void handle_set_position(GDBusConnection *connection, const gchar *sender,
                                const gchar *object_path, const gchar *interface_name,
                                const gchar *method_name, GVariant *parameters,
                                GDBusMethodInvocation *invocation, gpointer user_data)
{
}

static void handle_method_call(GDBusConnection *connection, const gchar *sender,
                               const gchar *object_path, const gchar *interface_name,
                               const gchar *method_name, GVariant *parameters,
                               GDBusMethodInvocation *invocation, gpointer user_data)
{
        if (g_strcmp0(method_name, "PlayPause") == 0)
        {
                handle_play_pause(connection, sender, object_path, interface_name,
                                  method_name, parameters, invocation, user_data);
        }
        else if (g_strcmp0(method_name, "Next") == 0)
        {
                handle_next(connection, sender, object_path, interface_name,
                            method_name, parameters, invocation, user_data);
        }
        else if (g_strcmp0(method_name, "Previous") == 0)
        {
                handle_previous(connection, sender, object_path, interface_name,
                                method_name, parameters, invocation, user_data);
        }
        else if (g_strcmp0(method_name, "Pause") == 0)
        {
                handle_pause(connection, sender, object_path, interface_name,
                             method_name, parameters, invocation, user_data);
        }
        else if (g_strcmp0(method_name, "Stop") == 0)
        {
                handle_stop(connection, sender, object_path, interface_name,
                            method_name, parameters, invocation, user_data);
        }
        else if (g_strcmp0(method_name, "Play") == 0)
        {
                handle_play(connection, sender, object_path, interface_name,
                            method_name, parameters, invocation, user_data);
        }
        else if (g_strcmp0(method_name, "Seek") == 0)
        {
                handle_seek(connection, sender, object_path, interface_name,
                            method_name, parameters, invocation, user_data);
        }
        else if (g_strcmp0(method_name, "SetPosition") == 0)
        {
                handle_set_position(connection, sender, object_path, interface_name,
                                    method_name, parameters, invocation, user_data);
        }
        else if (g_strcmp0(method_name, "Raise") == 0)
        {
                handle_raise(connection, sender, object_path, interface_name,
                             method_name, parameters, invocation, user_data);
        }
        else if (g_strcmp0(method_name, "Quit") == 0)
        {
                handle_quit(connection, sender, object_path, interface_name,
                            method_name, parameters, invocation, user_data);
        }
        else
        {
                g_dbus_method_invocation_return_dbus_error(invocation,
                                                           "org.freedesktop.DBus.Error.UnknownMethod",
                                                           "No such method");
        }
}

// static void on_bus_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
// {
//         // g_print("Acquired bus name: %s\n", name);
// }

static void on_bus_name_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
        // g_print("Acquired bus name: %s\n", name);
}

static void on_bus_name_lost(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
        // g_print("Lost bus name: %s\n", name);
}

static gboolean get_playback_status(GDBusConnection *connection, const gchar *sender,
                                    const gchar *object_path, const gchar *interface_name,
                                    const gchar *property_name, GVariant **value,
                                    GError **error, gpointer user_data)
{
        const gchar *status = "Stopped";

        if (isPaused())
        {
                status = "Paused";
        }
        else if (currentSong == NULL)
        {
                status = "Stopped";
        }
        else
        {
                status = "Playing";
        }
        *value = g_variant_new_string(status);
        return TRUE;
}

static gboolean get_loop_status(GDBusConnection *connection, const gchar *sender,
                                const gchar *object_path, const gchar *interface_name,
                                const gchar *property_name, GVariant **value,
                                GError **error, gpointer user_data)
{
        *value = g_variant_new_string(LoopStatus);
        return TRUE;
}

static gboolean get_rate(GDBusConnection *connection, const gchar *sender,
                         const gchar *object_path, const gchar *interface_name,
                         const gchar *property_name, GVariant **value,
                         GError **error, gpointer user_data)
{
        *value = g_variant_new_double(Rate);
        return TRUE;
}

static gboolean get_shuffle(GDBusConnection *connection, const gchar *sender,
                            const gchar *object_path, const gchar *interface_name,
                            const gchar *property_name, GVariant **value,
                            GError **error, gpointer user_data)
{
        *value = g_variant_new_boolean(isShuffleEnabled() ? TRUE : FALSE);
        return TRUE;
}

static gboolean get_metadata(GDBusConnection *connection, const gchar *sender,
                             const gchar *object_path, const gchar *interface_name,
                             const gchar *property_name, GVariant **value,
                             GError **error, gpointer user_data)
{
        SongData *currentSongData = getCurrentSongData();

        GVariantBuilder metadata_builder;
        g_variant_builder_init(&metadata_builder, G_VARIANT_TYPE_DICTIONARY);

        if (currentSong != NULL && currentSongData != NULL && currentSongData->metadata != NULL)
        {
                g_variant_builder_add(&metadata_builder, "{sv}", "xesam:title", g_variant_new_string(currentSongData->metadata->title));

                // Build list of strings for artist
                const gchar *artistList[2];
                if (currentSongData->metadata->artist[0] != '\0')
                {
                        artistList[0] = currentSongData->metadata->artist;
                        artistList[1] = NULL;
                }
                else
                {
                        artistList[0] = "";
                        artistList[1] = NULL;
                }

                gchar *coverArtUrl = g_strdup_printf("file://%s", currentSongData->coverArtPath);

                g_variant_builder_add(&metadata_builder, "{sv}", "xesam:artist", g_variant_new_strv(artistList, -1));
                g_variant_builder_add(&metadata_builder, "{sv}", "xesam:album", g_variant_new_string(currentSongData->metadata->album));
                g_variant_builder_add(&metadata_builder, "{sv}", "xesam:contentCreated", g_variant_new_string(currentSongData->metadata->date));
                g_variant_builder_add(&metadata_builder, "{sv}", "mpris:artUrl", g_variant_new_string(coverArtUrl));
                g_variant_builder_add(&metadata_builder, "{sv}", "mpris:trackid", g_variant_new_object_path(currentSongData->trackId));

                gint64 length = llround((*currentSongData->duration) * G_USEC_PER_SEC);
                g_variant_builder_add(&metadata_builder, "{sv}", "mpris:length", g_variant_new_int64(length));

                g_free(coverArtUrl);
        }
        else
        {
                g_variant_builder_add(&metadata_builder, "{sv}", "xesam:title", g_variant_new_string(""));
                g_variant_builder_add(&metadata_builder, "{sv}", "xesam:artist", g_variant_new_strv((const gchar *[]){"", NULL}, -1));
                g_variant_builder_add(&metadata_builder, "{sv}", "xesam:album", g_variant_new_string(""));
                g_variant_builder_add(&metadata_builder, "{sv}", "xesam:contentCreated", g_variant_new_string(""));
                g_variant_builder_add(&metadata_builder, "{sv}", "mpris:artUrl", g_variant_new_string(""));
                g_variant_builder_add(&metadata_builder, "{sv}", "mpris:trackid", g_variant_new_object_path("/org/mpris/MediaPlayer2/TrackList/NoTrack"));

                gint64 placeholderLength = 0;
                g_variant_builder_add(&metadata_builder, "{sv}", "mpris:length", g_variant_new_int64(placeholderLength));
        }

        GVariant *metadata_variant = g_variant_builder_end(&metadata_builder);
        *value = g_variant_ref_sink(metadata_variant);
        return TRUE;
}

static gboolean get_volume(GDBusConnection *connection, const gchar *sender,
                           const gchar *object_path, const gchar *interface_name,
                           const gchar *property_name, GVariant **value,
                           GError **error, gpointer user_data)
{

        Volume = (gdouble)getCurrentVolume();

        *value = g_variant_new_double(Volume);
        return TRUE;
}

static gboolean get_position(GDBusConnection *connection, const gchar *sender,
                             const gchar *object_path, const gchar *interface_name,
                             const gchar *property_name, GVariant **value,
                             GError **error, gpointer user_data)
{
        // Convert elapsedSeconds from milliseconds to microseconds
        gint64 positionMicroseconds = llround(elapsedSeconds * G_USEC_PER_SEC);

        *value = g_variant_new_int64(positionMicroseconds);

        return TRUE;
}

static gboolean get_minimum_rate(GDBusConnection *connection, const gchar *sender,
                                 const gchar *object_path, const gchar *interface_name,
                                 const gchar *property_name, GVariant **value,
                                 GError **error, gpointer user_data)
{
        *value = g_variant_new_double(MinimumRate);
        return TRUE;
}

static gboolean get_maximum_rate(GDBusConnection *connection, const gchar *sender,
                                 const gchar *object_path, const gchar *interface_name,
                                 const gchar *property_name, GVariant **value,
                                 GError **error, gpointer user_data)
{
        *value = g_variant_new_double(MaximumRate);
        return TRUE;
}

static gboolean get_can_go_next(GDBusConnection *connection, const gchar *sender,
                                const gchar *object_path, const gchar *interface_name,
                                const gchar *property_name, GVariant **value,
                                GError **error, gpointer user_data)
{

        CanGoNext = (currentSong == NULL || currentSong->next != NULL) ? TRUE : FALSE;

        *value = g_variant_new_boolean(CanGoNext);
        return TRUE;
}

static gboolean get_can_go_previous(GDBusConnection *connection, const gchar *sender,
                                    const gchar *object_path, const gchar *interface_name,
                                    const gchar *property_name, GVariant **value,
                                    GError **error, gpointer user_data)
{

        CanGoPrevious = (currentSong == NULL || currentSong->prev != NULL) ? TRUE : FALSE;

        *value = g_variant_new_boolean(CanGoPrevious);
        return TRUE;
}

static gboolean get_can_play(GDBusConnection *connection, const gchar *sender,
                             const gchar *object_path, const gchar *interface_name,
                             const gchar *property_name, GVariant **value,
                             GError **error, gpointer user_data)
{
        *value = g_variant_new_boolean(CanPlay);
        return TRUE;
}

static gboolean get_can_pause(GDBusConnection *connection, const gchar *sender,
                              const gchar *object_path, const gchar *interface_name,
                              const gchar *property_name, GVariant **value,
                              GError **error, gpointer user_data)
{
        *value = g_variant_new_boolean(CanPause);
        return TRUE;
}

static gboolean get_can_seek(GDBusConnection *connection, const gchar *sender,
                             const gchar *object_path, const gchar *interface_name,
                             const gchar *property_name, GVariant **value,
                             GError **error, gpointer user_data)
{
        *value = g_variant_new_boolean(CanSeek);
        return TRUE;
}

static gboolean get_can_control(GDBusConnection *connection, const gchar *sender,
                                const gchar *object_path, const gchar *interface_name,
                                const gchar *property_name, GVariant **value,
                                GError **error, gpointer user_data)
{
        *value = g_variant_new_boolean(CanControl);
        return TRUE;
}

static GVariant *get_property_callback(GDBusConnection *connection, const gchar *sender,
                                       const gchar *object_path, const gchar *interface_name,
                                       const gchar *property_name, GError **error, gpointer user_data)
{

        GVariant *value = NULL;

        if (g_strcmp0(property_name, "PlaybackStatus") == 0)
        {
                get_playback_status(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        }
        else if (g_strcmp0(property_name, "LoopStatus") == 0)
        {
                get_loop_status(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        }
        else if (g_strcmp0(property_name, "Rate") == 0)
        {
                get_rate(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        }
        else if (g_strcmp0(property_name, "Shuffle") == 0)
        {
                get_shuffle(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        }
        else if (g_strcmp0(property_name, "Metadata") == 0)
        {
                get_metadata(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        }
        else if (g_strcmp0(property_name, "Volume") == 0)
        {
                get_volume(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        }
        else if (g_strcmp0(property_name, "Position") == 0)
        {
                get_position(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        }
        else if (g_strcmp0(property_name, "MinimumRate") == 0)
        {
                get_minimum_rate(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        }
        else if (g_strcmp0(property_name, "MaximumRate") == 0)
        {
                get_maximum_rate(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        }
        else if (g_strcmp0(property_name, "CanGoNext") == 0)
        {
                get_can_go_next(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        }
        else if (g_strcmp0(property_name, "CanGoPrevious") == 0)
        {
                get_can_go_previous(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        }
        else if (g_strcmp0(property_name, "CanPlay") == 0)
        {
                get_can_play(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        }
        else if (g_strcmp0(property_name, "CanPause") == 0)
        {
                get_can_pause(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        }
        else if (g_strcmp0(property_name, "CanSeek") == 0)
        {
                get_can_seek(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        }
        else if (g_strcmp0(property_name, "CanControl") == 0)
        {
                get_can_control(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        }
        else if (g_strcmp0(property_name, "DesktopIconName") == 0)
        {
                get_desktop_icon_name(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        }
        else if (g_strcmp0(property_name, "DesktopEntry") == 0)
        {
                get_desktop_entry(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        }
        else if (g_strcmp0(property_name, "Identity") == 0)
        {
                get_identity(connection, sender, object_path, interface_name, property_name, &value, error, user_data);
        }
        else
        {
                g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "Unknown property");
        }

        // Check if value is NULL and set an error if needed
        if (value == NULL && error == NULL)
        {
                g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "Property value is NULL");
        }

        return value;
}

static gboolean set_property_callback(GDBusConnection *connection, const gchar *sender,
                                      const gchar *object_path, const gchar *interface_name,
                                      const gchar *property_name, GVariant *value,
                                      GError **error, gpointer user_data)
{

        if (g_strcmp0(interface_name, "org.mpris.MediaPlayer2.Player") == 0)
        {
                if (g_strcmp0(property_name, "PlaybackStatus") == 0)
                {
                        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "Setting PlaybackStatus property not supported");
                        return FALSE;
                }
                else if (g_strcmp0(property_name, "Volume") == 0)
                {
                        double new_volume;
                        g_variant_get(value, "d", &new_volume);
                        setVolume((int)new_volume);
                        return TRUE;
                }
                else if (g_strcmp0(property_name, "LoopStatus") == 0)
                {
                        toggleRepeat();
                        return TRUE;
                }
                else if (g_strcmp0(property_name, "Shuffle") == 0)
                {
                        toggleShuffle();
                        return TRUE;
                }
                else if (g_strcmp0(property_name, "Position") == 0)
                {
                        // gint64 new_position;
                        // g_variant_get(value, "x", &new_position);
                        // set_position(new_position);
                        // return TRUE;
                        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "Setting Position property not supported");
                        return FALSE;
                }
                else
                {
                        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "Setting property not supported");
                        return FALSE;
                }
        }
        else
        {
                g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "Unknown interface");
                return FALSE;
        }
}

// MPRIS MediaPlayer2 interface vtable
static const GDBusInterfaceVTable media_player_interface_vtable = {
    .method_call = handle_method_call,     // We're using individual method handlers
    .get_property = get_property_callback, // Handle the property getters individually
    .set_property = set_property_callback,
    .padding = {
        handle_raise,
        handle_quit}};

// MPRIS Player interface vtable
static const GDBusInterfaceVTable player_interface_vtable = {
    .method_call = handle_method_call,     // We're using individual method handlers
    .get_property = get_property_callback, // Handle the property getters individually
    .set_property = set_property_callback,
    .padding = {
        handle_next,
        handle_previous,
        handle_pause,
        handle_play_pause,
        handle_stop,
        handle_play,
        handle_seek,
        handle_set_position}};

void emitPlaybackStoppedMpris()
{
        if (connection)
        {
                g_dbus_connection_call(connection,
                                       NULL,
                                       "/org/mpris/MediaPlayer2",
                                       "org.freedesktop.DBus.Properties",
                                       "Set",
                                       g_variant_new("(ssv)", "org.mpris.MediaPlayer2.Player", "PlaybackStatus", g_variant_new_string("Stopped")),
                                       G_VARIANT_TYPE("(v)"),
                                       G_DBUS_CALL_FLAGS_NONE,
                                       -1,
                                       NULL,
                                       NULL,
                                       NULL);
        }
}

void cleanupMpris()
{
        if (registration_id > 0)
        {
                g_dbus_connection_unregister_object(connection, registration_id);
                registration_id = -1;
        }

        if (player_registration_id > 0)
        {
                g_dbus_connection_unregister_object(connection, player_registration_id);
                player_registration_id = -1;
        }

        if (bus_name_id > 0)
        {
                g_bus_unown_name(bus_name_id);
                bus_name_id = -1;
        }

        if (connection != NULL)
        {
                g_object_unref(connection);
                connection = NULL;
        }

        if (global_main_context != NULL)
        {
                g_main_context_unref(global_main_context);
                global_main_context = NULL;
        }
}

void initMpris()
{
        if (global_main_context == NULL)
        {
                global_main_context = g_main_context_new();
        }

        GDBusNodeInfo *introspection_data = g_dbus_node_info_new_for_xml(introspection_xml, NULL);
        connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);

        if (!connection)
        {
                g_dbus_node_info_unref(introspection_data);
                g_printerr("Failed to connect to D-Bus\n");
                exit(0);
        }

        const char *app_name = "org.mpris.MediaPlayer2.kew";
       
        char unique_name[256];
        snprintf(unique_name, sizeof(unique_name), "%s%d", app_name, getpid());

        GError *error = NULL;
        bus_name_id = g_bus_own_name_on_connection(connection,
                                                   unique_name,
                                                   G_BUS_NAME_OWNER_FLAGS_NONE,
                                                   on_bus_name_acquired,
                                                   on_bus_name_lost,
                                                   NULL,
                                                   NULL);

        if (bus_name_id == 0)
        {
                printf("Failed to own D-Bus name: %s\n", unique_name);
                exit(0);
        }

        registration_id = g_dbus_connection_register_object(
            connection,
            "/org/mpris/MediaPlayer2",
            introspection_data->interfaces[0],
            &media_player_interface_vtable,
            NULL,
            NULL,
            &error);

        if (!registration_id)
        {
                g_dbus_node_info_unref(introspection_data);
                g_printerr("Failed to register media player object: %s\n", error->message);
                g_error_free(error);
                exit(0);
        }

        player_registration_id = g_dbus_connection_register_object(
            connection,
            "/org/mpris/MediaPlayer2",
            introspection_data->interfaces[1],
            &player_interface_vtable,
            NULL,
            NULL,
            &error);

        if (!player_registration_id)
        {
                g_dbus_node_info_unref(introspection_data);
                g_printerr("Failed to register media player object: %s\n", error->message);
                g_error_free(error);
                exit(0);
        }

        g_dbus_node_info_unref(introspection_data);
}

void emitStartPlayingMpris()
{
        GVariant *parameters = g_variant_new("(s)", "Playing");
        g_dbus_connection_emit_signal(connection,
                                      NULL,
                                      "/org/mpris/MediaPlayer2",
                                      "org.mpris.MediaPlayer2.Player",
                                      "PlaybackStatusChanged",
                                      parameters,
                                      NULL);
}

void emitMetadataChanged(const gchar *title, const gchar *artist, const gchar *album, const gchar *coverArtPath, const gchar *trackId, Node *currentSong, gint64 length)
{
        gchar *coverArtUrl = g_strdup_printf("file://%s", coverArtPath);

        GVariantBuilder metadata_builder;
        g_variant_builder_init(&metadata_builder, G_VARIANT_TYPE_DICTIONARY);
        g_variant_builder_add(&metadata_builder, "{sv}", "xesam:title", g_variant_new_string(title));

        const gchar *artistList[2];
        if (artist)
        {
                artistList[0] = artist;
                artistList[1] = NULL;
        }
        else
        {
                artistList[0] = "";
                artistList[1] = NULL;
        }

        g_variant_builder_add(&metadata_builder, "{sv}", "xesam:artist", g_variant_new_strv(artistList, -1));
        g_variant_builder_add(&metadata_builder, "{sv}", "xesam:album", g_variant_new_string(album));
        g_variant_builder_add(&metadata_builder, "{sv}", "mpris:artUrl", g_variant_new_string(coverArtUrl));
        g_variant_builder_add(&metadata_builder, "{sv}", "mpris:trackid", g_variant_new_object_path(trackId));
        g_variant_builder_add(&metadata_builder, "{sv}", "mpris:length", g_variant_new_int64(length));

        GVariantBuilder changed_properties_builder;
        g_variant_builder_init(&changed_properties_builder, G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_add(&changed_properties_builder, "{sv}", "Metadata", g_variant_builder_end(&metadata_builder));
        g_variant_builder_add(&changed_properties_builder, "{sv}", "CanGoPrevious", g_variant_new_boolean((currentSong != NULL && currentSong->prev != NULL)));
        g_variant_builder_add(&changed_properties_builder, "{sv}", "CanGoNext", g_variant_new_boolean((currentSong != NULL && currentSong->next != NULL)));

        g_dbus_connection_emit_signal(connection, NULL, "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "PropertiesChanged",
                                      g_variant_new("(sa{sv}as)", "org.mpris.MediaPlayer2.Player", &changed_properties_builder, NULL), NULL);

        g_variant_builder_clear(&metadata_builder);
        g_variant_builder_clear(&changed_properties_builder);

        g_free(coverArtUrl);
}
