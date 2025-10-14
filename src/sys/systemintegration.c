/**
 * @file systemintegration.c
 * @brief System-level integrations and OS interop.
 *
 * Provides hooks for platform-specific features such as
 * power management, clipboard access, or system event handling.
 */

#include "systemintegration.h"

#include "common/common.h"

#include "mpris.h"
#include "notifications.h"

#include "utils/file.h"
#include "utils/term.h"
#include "utils/utils.h"

#include "gio/gio.h"
#include "math.h"
#include <stdio.h>
#include <sys/wait.h>

static GDBusConnection *connection = NULL;
static GMainContext *globalMainContext = NULL;

void setGMainContext(GMainContext *val)
{
        globalMainContext = val;
}

void *getGMainContext(void)
{
        return globalMainContext;
}

GDBusConnection *getGDBusConnection(void)
{
        return connection;
}

void setGDBusConnection(GDBusConnection *val)
{
        connection = val;
}

void processDBusEvents(void)
{
        while (g_main_context_pending(getGMainContext()))
        {
                g_main_context_iteration(getGMainContext(), FALSE);
        }
}

void resize(UIState *uis)
{
        alarm(1); // Timer
        while (uis->resizeFlag)
        {
                uis->resizeFlag = 0;
                c_sleep(100);
        }
        alarm(0); // Cancel timer
        printf("\033[1;1H");
        clearScreen();
        triggerRefresh();
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

void notifyMPRISSwitch(SongData *currentSongData)
{
        if (currentSongData == NULL)
                return;

        gint64 length = getLengthInMicroSec(currentSongData->duration);

        // Update mpris
        emitMetadataChanged(
            currentSongData->metadata->title, currentSongData->metadata->artist,
            currentSongData->metadata->album, currentSongData->coverArtPath,
            currentSongData->trackId != NULL ? currentSongData->trackId : "",
            getCurrentSong(), length);
}

void notifySongSwitch(SongData *currentSongData)
{
        AppState *state = getAppState();
        UISettings *ui = &(state->uiSettings);
        if (currentSongData != NULL && currentSongData->hasErrors == 0 &&
            currentSongData->metadata &&
            strnlen(currentSongData->metadata->title, 10) > 0)
        {
#ifdef USE_DBUS
                displaySongNotification(currentSongData->metadata->artist,
                                        currentSongData->metadata->title,
                                        currentSongData->coverArtPath, ui);
#else
                (void)ui;
#endif

                notifyMPRISSwitch(currentSongData);

                Node *current = getCurrentSong();

                if (current != NULL)
                        state->uiState.lastNotifiedId = current->id;
        }
}

int isProcessRunning(pid_t pid)
{
        if (pid <= 0)
        {
                return 0; // Invalid PID
        }

        // Send signal 0 to check if the process exists
        if (kill(pid, 0) == 0)
        {
                return 1; // Process exists
        }

        // Check errno for detailed status
        if (errno == ESRCH)
        {
                return 0; // No such process
        }
        else if (errno == EPERM)
        {
                return 1; // Process exists but we don't have permission
        }

        return 0; // Other errors
}

int isKewProcess(pid_t pid)
{
        char comm_path[64];
        char process_name[256];
        FILE *file;

        // First check /proc/[pid]/comm for the process name
        snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", pid);
        file = fopen(comm_path, "r");
        if (file != NULL)
        {
                if (fgets(process_name, sizeof(process_name), file))
                {
                        fclose(file);
                        // Remove trailing newline
                        process_name[strcspn(process_name, "\n")] = 0;

                        // Check if it's kew (process name might be truncated to
                        // 15 chars)
                        if (strstr(process_name, "kew") != NULL)
                        {
                                return 1; // It's likely kew
                        }
                }
                else
                {
                        fclose(file);
                }
        }

        return 0; // Not kew or couldn't determine
}

void deletePidFile()
{
        char pidfilePath[MAXPATHLEN];
        const char *temp_dir = getTempDir();

        snprintf(pidfilePath, sizeof(pidfilePath), "%s/kew_%d.pid", temp_dir,
                 getuid());

        FILE *pidfile;

        pidfile = fopen(pidfilePath, "r");

        if (pidfile != NULL)
        {
                fclose(pidfile);
                unlink(pidfilePath);
        }
}

pid_t readPidFile()
{
        char pidfilePath[MAXPATHLEN];
        const char *temp_dir = getTempDir();

        snprintf(pidfilePath, sizeof(pidfilePath), "%s/kew_%d.pid", temp_dir,
                 getuid());

        FILE *pidfile;
        pid_t pid;

        pidfile = fopen(pidfilePath, "r");
        if (pidfile != NULL)
        {
                if (fscanf(pidfile, "%d", &pid) == 1)
                {
                        fclose(pidfile);
                }
        }
        else
        {
                {
                        pid = -1;
                        unlink(pidfilePath);
                }
        }

        return pid;
}

void createPidFile()
{
        char pidfilePath[MAXPATHLEN];
        const char *temp_dir = getTempDir();

        snprintf(pidfilePath, sizeof(pidfilePath), "%s/kew_%d.pid", temp_dir,
                 getuid());

        FILE *pidfile = fopen(pidfilePath, "w");
        if (pidfile == NULL)
        {
                perror("Unable to create PID file");
                exit(1);
        }

        fprintf(pidfile, "%d\n", getpid());
        fclose(pidfile);
}

void restartKew(char *argv[])
{
    pid_t oldpid = readPidFile();
    if (oldpid > 0)
    {
        if (kill(oldpid, SIGUSR1) != 0)
        {
            if (errno == ESRCH)
            {
                fprintf(stderr, "No running kew process found.\n");
                deletePidFile();
            }
            else
            {
                fprintf(stderr, "Failed to stop old kew (pid %d): %s\n",
                        oldpid, strerror(errno));
            }
        }
        else
        {
            int status;
            if (waitpid(oldpid, &status, 0) == -1 && errno != ECHILD)
            {
                perror("waitpid");
            }
            deletePidFile();
        }
    }

    execvp("kew", argv);

    fprintf(stderr, "Failed to restart kew via execvp: %s\n", strerror(errno));
    exit(1);
}

void handleShutdown(int sig)
{
        (void)sig;
        exit(0); // runs all atexit handlers
}

// Ensures only a single instance of kew can run at a time for the current user.
void restartIfAlreadyRunning(char *argv[])
{
        signal(SIGUSR1, handleShutdown);

        pid_t pid = readPidFile();

#ifdef __ANDROID__
        if (isProcessRunning(pid) && isKewProcess(pid))
#else
        if (isProcessRunning(pid))
#endif
        {
                restartKew(argv);
        }
        else
        {
                deletePidFile();
        }

        createPidFile();
}

void handleResize(int sig)
{
        (void)sig;
        AppState *state = getAppState();
        state->uiState.resizeFlag = 1;
}

void resetResizeFlag(int sig)
{
        (void)sig;
        AppState *state = getAppState();
        state->uiState.resizeFlag = 0;
}

void initResize(void)
{
        signal(SIGWINCH, handleResize);

        struct sigaction sa;
        sa.sa_handler = resetResizeFlag;
        sigemptyset(&(sa.sa_mask));
        sa.sa_flags = 0;
        sigaction(SIGALRM, &sa, NULL);
}


void quit(void) { exit(0); }
