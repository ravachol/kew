#include "systemintegration.h"
#include "math.h"
#include <stdio.h>
#include "gio/gio.h"

static GDBusConnection *connection = NULL;

GDBusConnection *getGDBusConnection(void)
{
        return connection;
}

void setGDBusConnection(GDBusConnection *val)
{
        connection = val;
}

void emitStringPropertyChanged(const gchar *propertyName, const gchar *newValue)
{
#ifndef __APPLE__
        GVariantBuilder changed_properties_builder;
        g_variant_builder_init(&changed_properties_builder, G_VARIANT_TYPE("a{sv}"));

        GVariant *value_variant = g_variant_new_string(newValue);
        g_variant_builder_add(&changed_properties_builder, "{sv}", propertyName, value_variant);

        GVariant *signal_variant =
            g_variant_new("(sa{sv}as)", "org.mpris.MediaPlayer2.Player", &changed_properties_builder, NULL);

        g_variant_ref_sink(signal_variant);

        g_dbus_connection_emit_signal(
            connection, NULL, "/org/mpris/MediaPlayer2",
            "org.freedesktop.DBus.Properties", "PropertiesChanged",
            signal_variant, NULL);

        g_variant_builder_clear(&changed_properties_builder);
        g_variant_unref(signal_variant);
#else
        (void)propertyName;
        (void)newValue;
#endif
}

void updatePlaybackPosition(double elapsedSeconds)
{
#ifndef __APPLE__
        if (elapsedSeconds < 0.0)
                elapsedSeconds = 0.0;

        // Max safe seconds to avoid overflow when multiplied by 1,000,000
        const double maxSeconds = (double)(LLONG_MAX / G_USEC_PER_SEC);

        if (elapsedSeconds > maxSeconds)
                elapsedSeconds = maxSeconds;

        GVariantBuilder changedPropertiesBuilder;
        g_variant_builder_init(&changedPropertiesBuilder,
                               G_VARIANT_TYPE_DICTIONARY);
        g_variant_builder_add(
            &changedPropertiesBuilder, "{sv}", "Position",
            g_variant_new_int64(llround(elapsedSeconds * G_USEC_PER_SEC)));

        GVariant *parameters =
            g_variant_new("(sa{sv}as)", "org.mpris.MediaPlayer2.Player",
                          &changedPropertiesBuilder, NULL);

        g_dbus_connection_emit_signal(connection, NULL,
                                      "/org/mpris/MediaPlayer2",
                                      "org.freedesktop.DBus.Properties",
                                      "PropertiesChanged", parameters, NULL);
#else
        (void)elapsedSeconds;
#endif
}

void emitSeekedSignal(double newPositionSeconds)
{
#ifndef __APPLE__
        if (newPositionSeconds < 0.0)
                newPositionSeconds = 0.0;

        const double maxSeconds = (double)(LLONG_MAX / G_USEC_PER_SEC);
        if (newPositionSeconds > maxSeconds)
                newPositionSeconds = maxSeconds;

        gint64 newPositionMicroseconds =
            llround(newPositionSeconds * G_USEC_PER_SEC);

        GVariant *parameters = g_variant_new("(x)", newPositionMicroseconds);

        g_dbus_connection_emit_signal(
            connection, NULL, "/org/mpris/MediaPlayer2",
            "org.mpris.MediaPlayer2.Player", "Seeked", parameters, NULL);
#else
        (void)newPositionSeconds;
#endif
}

void emitBooleanPropertyChanged(const gchar *propertyName, gboolean newValue)
{
#ifndef __APPLE__
        GVariantBuilder changed_properties_builder;
        g_variant_builder_init(&changed_properties_builder,
                               G_VARIANT_TYPE("a{sv}"));

        GVariant *value_variant = g_variant_new_boolean(newValue);
        if (value_variant == NULL)
        {
                fprintf(stderr,
                        "Failed to allocate GVariant for boolean property\n");
                return;
        }

        g_variant_builder_add(&changed_properties_builder, "{sv}", propertyName,
                              value_variant);

        GVariant *signal_variant =
            g_variant_new("(sa{sv}as)", "org.mpris.MediaPlayer2.Player",
                          &changed_properties_builder, NULL);
        if (signal_variant == NULL)
        {
                fprintf(stderr, "Failed to allocate GVariant for "
                                "PropertiesChanged signal\n");
                g_variant_builder_clear(&changed_properties_builder);
                return;
        }

        g_dbus_connection_emit_signal(
            connection, NULL, "/org/mpris/MediaPlayer2",
            "org.freedesktop.DBus.Properties", "PropertiesChanged",
            signal_variant, NULL);

        g_variant_builder_clear(&changed_properties_builder);
#else
        (void)propertyName;
        (void)newValue;
#endif
}

void quit(void) { exit(0); }
