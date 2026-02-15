/**
 * @file sys_integration.c
 * @brief System-level integrations and OS interop.
 *
 * Provides hooks for platform-specific features such as
 * power management, clipboard access, or system event handling.
 */

#include "sys_integration.h"

#include "common/common.h"

#include "discord_rpc.h"
#include "mpris.h"
#ifdef USE_MACOS_MEDIA
#include "macos_nowplaying.h"
#endif
#include "notifications.h"

#include "ui/player_ui.h"
#include "utils/file.h"
#include "utils/term.h"
#include "utils/utils.h"

#include "gio/gio.h"
#include "math.h"
#include <stdio.h>
#include <sys/wait.h>

static GDBusConnection *connection = NULL;
static GMainContext *global_main_context = NULL;
static volatile sig_atomic_t g_should_exit = 0;

void set_g_main_context(GMainContext *val)
{
        global_main_context = val;
}

void *get_g_main_context(void)
{
        return global_main_context;
}

GDBusConnection *get_gd_bus_connection(void)
{
        return connection;
}

void set_gd_bus_connection(GDBusConnection *val)
{
        connection = val;
}

void process_d_bus_events(void)
{
#ifdef USE_MACOS_MEDIA
        macos_process_events();
#endif
        while (g_main_context_pending(get_g_main_context())) {
                g_main_context_iteration(get_g_main_context(), FALSE);
        }
}

void resize(UIState *uis)
{
        alarm(1); // Timer
        while (uis->resizeFlag) {
                uis->resizeFlag = 0;
                c_sleep(100);
        }
        alarm(0); // Cancel timer
        printf("\033[1;1H");
        clear_screen();
        trigger_redraw_side_cover();
        trigger_refresh();
        set_term_size();
}

void emit_string_property_changed(const gchar *property_name, const gchar *new_value)
{
#ifdef USE_DBUS
        GVariantBuilder changed_properties_builder;
        g_variant_builder_init(&changed_properties_builder, G_VARIANT_TYPE("a{sv}"));

        GVariant *value_variant = g_variant_new_string(new_value);
        g_variant_builder_add(&changed_properties_builder, "{sv}", property_name, value_variant);

        GVariant *signal_variant =
            g_variant_new("(sa{sv}as)", "org.mpris.MediaPlayer2.Player", &changed_properties_builder, NULL);

        g_variant_ref_sink(signal_variant);

        g_dbus_connection_emit_signal(
            connection, NULL, "/org/mpris/MediaPlayer2",
            "org.freedesktop.DBus.Properties", "PropertiesChanged",
            signal_variant, NULL);

        g_variant_builder_clear(&changed_properties_builder);
        g_variant_unref(signal_variant);
#elif defined(USE_MACOS_MEDIA)
        if (strcmp(property_name, "PlaybackStatus") == 0) {
                if (strcmp(new_value, "Playing") == 0)
                        macos_set_playback_state_playing();
                else if (strcmp(new_value, "Paused") == 0)
                        macos_set_playback_state_paused();
                else if (strcmp(new_value, "Stopped") == 0)
                        macos_set_playback_state_stopped();
        }
#else
        (void)property_name;
        (void)new_value;
#endif
}

void update_playback_position(double elapsed_seconds)
{
#ifdef USE_DBUS
        if (elapsed_seconds < 0.0)
                elapsed_seconds = 0.0;

        // Max safe seconds to avoid overflow when multiplied by 1,000,000
        const double max_seconds = (double)(LLONG_MAX / G_USEC_PER_SEC);

        if (elapsed_seconds > max_seconds)
                elapsed_seconds = max_seconds;

        GVariantBuilder changedPropertiesBuilder;
        g_variant_builder_init(&changedPropertiesBuilder,
                               G_VARIANT_TYPE_DICTIONARY);
        g_variant_builder_add(
            &changedPropertiesBuilder, "{sv}", "Position",
            g_variant_new_int64(llround(elapsed_seconds * G_USEC_PER_SEC)));

        GVariant *parameters =
            g_variant_new("(sa{sv}as)", "org.mpris.MediaPlayer2.Player",
                          &changedPropertiesBuilder, NULL);

        g_dbus_connection_emit_signal(connection, NULL,
                                      "/org/mpris/MediaPlayer2",
                                      "org.freedesktop.DBus.Properties",
                                      "PropertiesChanged", parameters, NULL);
#elif defined(USE_MACOS_MEDIA)
        if (elapsed_seconds < 0.0)
                elapsed_seconds = 0.0;
        macos_update_playback_position(elapsed_seconds);
#else
        (void)elapsed_seconds;
#endif
}

void emit_seeked_signal(double new_position_seconds)
{
#ifdef USE_DBUS
        if (new_position_seconds < 0.0)
                new_position_seconds = 0.0;

        const double max_seconds = (double)(LLONG_MAX / G_USEC_PER_SEC);
        if (new_position_seconds > max_seconds)
                new_position_seconds = max_seconds;

        gint64 newPositionMicroseconds =
            llround(new_position_seconds * G_USEC_PER_SEC);

        GVariant *parameters = g_variant_new("(x)", newPositionMicroseconds);

        g_dbus_connection_emit_signal(
            connection, NULL, "/org/mpris/MediaPlayer2",
            "org.mpris.MediaPlayer2.Player", "Seeked", parameters, NULL);
#else
        (void)new_position_seconds;
#endif
}

void emit_boolean_property_changed(const gchar *property_name, gboolean new_value)
{
#ifdef USE_DBUS
        GVariantBuilder changed_properties_builder;
        g_variant_builder_init(&changed_properties_builder,
                               G_VARIANT_TYPE("a{sv}"));

        GVariant *value_variant = g_variant_new_boolean(new_value);
        if (value_variant == NULL) {
                fprintf(stderr,
                        "Failed to allocate GVariant for boolean property\n");
                return;
        }

        g_variant_builder_add(&changed_properties_builder, "{sv}", property_name,
                              value_variant);

        GVariant *signal_variant =
            g_variant_new("(sa{sv}as)", "org.mpris.MediaPlayer2.Player",
                          &changed_properties_builder, NULL);
        if (signal_variant == NULL) {
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
        (void)property_name;
        (void)new_value;
#endif
}

void notify_mpris_switch(SongData *current_song_data)
{
        if (current_song_data == NULL)
                return;

        gint64 length = get_length_in_micro_sec(current_song_data->duration);

        // Update mpris
        emit_metadata_changed(
            current_song_data->metadata->title, current_song_data->metadata->artist,
            current_song_data->metadata->album, current_song_data->cover_art_path,
            current_song_data->track_id != NULL ? current_song_data->track_id : "",
            get_current_song(), length);
}

void notify_song_switch(SongData *current_song_data)
{
        AppState *state = get_app_state();
        UISettings *ui = &(state->uiSettings);
        if (current_song_data != NULL && current_song_data->hasErrors == 0 &&
            current_song_data->metadata &&
            strnlen(current_song_data->metadata->title, 10) > 0) {
#ifdef USE_DBUS
                display_song_notification(current_song_data->metadata->artist,
                                          current_song_data->metadata->title,
                                          current_song_data->cover_art_path, ui);
#else
                (void)ui;
#endif

                notify_mpris_switch(current_song_data);

                if (ui->discordRPCEnabled)
                        notify_discord_switch(current_song_data);

                Node *current = get_current_song();

                if (current != NULL)
                        state->uiState.lastNotifiedId = current->id;
        }
}

int is_process_running(pid_t pid)
{
        if (pid <= 0) {
                return 0; // Invalid PID
        }

        // Send signal 0 to check if the process exists
        if (kill(pid, 0) == 0) {
                return 1; // Process exists
        }

        // Check errno for detailed status
        if (errno == ESRCH) {
                return 0; // No such process
        } else if (errno == EPERM) {
                return 1; // Process exists but we don't have permission
        }

        return 0; // Other errors
}

int is_kew_process(pid_t pid)
{
        char comm_path[64];
        char process_name[256];
        FILE *file;

        // First check /proc/[pid]/comm for the process name
        snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", pid);
        file = fopen(comm_path, "r");
        if (file != NULL) {
                if (fgets(process_name, sizeof(process_name), file)) {
                        fclose(file);
                        // Remove trailing newline
                        process_name[strcspn(process_name, "\n")] = 0;

                        // Check if it's kew (process name might be truncated to
                        // 15 chars)
                        if (strstr(process_name, "kew") != NULL) {
                                return 1; // It's likely kew
                        }
                } else {
                        fclose(file);
                }
        }

        return 0; // Not kew or couldn't determine
}

void delete_pid_file()
{
        char pidfile_path[PATH_MAX];
        const char *temp_dir = get_temp_dir();

        snprintf(pidfile_path, sizeof(pidfile_path), "%s/kew_%d.pid", temp_dir,
                 getuid());

        FILE *pidfile;

        pidfile = fopen(pidfile_path, "r");

        if (pidfile != NULL) {
                fclose(pidfile);
                unlink(pidfile_path);
        }
}

pid_t read_pid_file()
{
        char pidfile_path[PATH_MAX];
        const char *temp_dir = get_temp_dir();

        snprintf(pidfile_path, sizeof(pidfile_path), "%s/kew_%d.pid", temp_dir,
                 getuid());

        FILE *pidfile;
        pid_t pid;

        pidfile = fopen(pidfile_path, "r");
        if (pidfile != NULL) {
                if (fscanf(pidfile, "%d", &pid) == 1) {
                        fclose(pidfile);
                }
        } else {
                {
                        pid = -1;
                        unlink(pidfile_path);
                }
        }

        return pid;
}

void create_pid_file()
{
        char pidfile_path[PATH_MAX];
        const char *temp_dir = get_temp_dir();

        snprintf(pidfile_path, sizeof(pidfile_path), "%s/kew_%d.pid", temp_dir,
                 getuid());

        FILE *pidfile = fopen(pidfile_path, "w");
        if (pidfile == NULL) {
                perror("Unable to create PID file");
                exit(1);
        }

        fprintf(pidfile, "%d\n", getpid());
        fclose(pidfile);
}

void restart_kew(char *argv[])
{
    pid_t oldpid = read_pid_file();

    if (oldpid > 0) {
        // Check if process exists
        if (kill(oldpid, 0) == 0) {

            // Ask old instance to shut down cleanly
            if (kill(oldpid, SIGUSR1) != 0) {
                fprintf(stderr,
                        "Failed to signal old kew (pid %d): %s\n",
                        oldpid, strerror(errno));
            } else {
                // Wait up to 5 seconds for it to exit
                int retries = 50;  // 50 × 100ms = 5s

                while (retries-- > 0) {
                    if (kill(oldpid, 0) == -1 && errno == ESRCH) {
                        break;  // Process is gone
                    }
                    usleep(100000); // 100ms
                }

                // If still running, force kill
                if (kill(oldpid, 0) == 0) {
                    fprintf(stderr,
                            "Old kew (pid %d) did not exit in time. Forcing termination.\n",
                            oldpid);

                    kill(oldpid, SIGKILL);

                    // Give it a moment to die
                    usleep(200000);
                }
            }
        } else if (errno == ESRCH) {
            fprintf(stderr, "No running kew process found.\n");
        } else {
            fprintf(stderr,
                    "Error checking old kew (pid %d): %s\n",
                    oldpid, strerror(errno));
        }

        delete_pid_file();
    }

    // Replace current process with new kew instance
    execvp("kew", argv);

    // Only reached if exec fails
    fprintf(stderr,
            "Failed to restart kew via execvp: %s\n",
            strerror(errno));
    _exit(1);
}

// Ensures only a single instance of kew can run at a time for the current user.
void restart_if_already_running(char *argv[])
{
        signal(SIGUSR1, handle_exit_signal);

        pid_t pid = read_pid_file();

#ifdef __ANDROID__
        if (is_process_running(pid) && is_kew_process(pid))
#else
        if (is_process_running(pid))
#endif
        {
                restart_kew(argv);
        } else {
                delete_pid_file();
        }

        create_pid_file();
}

void handle_resize(int sig)
{
        (void)sig;
        AppState *state = get_app_state();
        state->uiState.resizeFlag = 1;
}

void reset_resize_flag(int sig)
{
        (void)sig;
        AppState *state = get_app_state();
        state->uiState.resizeFlag = 0;
}

void init_resize(void)
{
        signal(SIGWINCH, handle_resize);

        struct sigaction sa;
        sa.sa_handler = reset_resize_flag;
        sigemptyset(&(sa.sa_mask));
        sa.sa_flags = 0;
        sigaction(SIGALRM, &sa, NULL);
}
