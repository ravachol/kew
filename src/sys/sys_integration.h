/**
 * @file sys_integration.h
 * @brief System-level integrations and OS interop.
 *
 * Provides hooks for platform-specific features such as
 * power management, clipboard access, or system event handling.
 */

#ifndef SYS_INTEGRATION_H
#define SYS_INTEGRATION_H

#include "common/appstate.h"

#include "gio/gio.h"
#include "glib.h"

void set_g_main_context(GMainContext *val);
void *get_g_main_context(void);
void set_g_d_bus_connection(GDBusConnection *val);
void emit_string_property_changed(const gchar *property_name, const gchar *new_value);
void update_playback_position(double elapsed_seconds);
void emit_seeked_signal(double new_position_seconds);
void emit_boolean_property_changed(const gchar *property_name, gboolean new_value);
void notify_mpris_switch(SongData *current_song_data);
void notify_song_switch(SongData *current_song_data);
void process_d_bus_events(void);
void resize(UIState *uis);
void restart_if_already_running(char *argv[]);
void restart_kew(char *argv[]);
void init_resize(void);
void quit(void);
GDBusConnection *get_g_d_bus_connection(void);

#endif
