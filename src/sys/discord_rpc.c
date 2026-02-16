/**
 * @file discord_rpc.c
 * @brief Integration that shows kew status in discord.
 *
 * Implements a discord RPC integration,

 */
#include "discord_rpc.h"
#include <gio/gio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define DISCORD_CLIENT_ID "1472901833633693696"
#define DISCORD_OP_HANDSHAKE 0
#define DISCORD_OP_FRAME 1

static GSocketConnection *discord_conn = NULL;
static GOutputStream *discord_out = NULL;
static gboolean discord_available = FALSE;
static gint64 track_start_time = 0;

void json_escape(const char *src, char *dst, size_t size)
{
        size_t j = 0;

        for (size_t i = 0; src[i] && j < size - 2; i++) {
                char c = src[i];

                if (c == '"' || c == '\\') {
                        if (j < size - 2) {
                                dst[j++] = '\\';
                                dst[j++] = c;
                        }
                } else if (c == '\n') {
                        if (j < size - 2) {
                                dst[j++] = '\\';
                                dst[j++] = 'n';
                        }
                } else {
                        dst[j++] = c;
                }
        }

        dst[j] = '\0';
}

// Send a frame over IPC
static void send_frame(int opcode, const char *json)
{
        if (!discord_available || !discord_out || !json)
                return;

        guint32 header[2];
        header[0] = GUINT32_TO_LE((guint32)opcode);
        header[1] = GUINT32_TO_LE((guint32)strlen(json));

        gsize written;
        GError *error = NULL;

        if (!g_output_stream_write_all(discord_out, header, 8, &written, NULL, &error) ||
            !g_output_stream_write_all(discord_out, json, strlen(json), &written, NULL, &error)) {
                discord_available = FALSE;
                g_clear_object(&discord_conn);
                discord_out = NULL;
                if (error)
                        g_error_free(error);
        }
}

static gboolean try_connect_socket(void)
{
        if (discord_conn)
                return TRUE;

#if defined(__linux__) || defined(__FreeBSD__) || defined(__APPLE__)

#if defined(__APPLE__)
        const char *runtime = g_build_filename(g_get_home_dir(), "Library", "Application Support", "Discord", NULL);
#else
        const char *runtime = g_getenv("XDG_RUNTIME_DIR");
#endif
        if (!runtime)
                return FALSE;

        for (int i = 0; i < 10; i++) {
                gchar *path = g_strdup_printf("%s/discord-ipc-%d", runtime, i);
                GSocketClient *client = g_socket_client_new();
                GError *error = NULL;

                GSocketAddress *address = g_unix_socket_address_new(path);
                GSocketConnection *conn = g_socket_client_connect(
                    client,
                    G_SOCKET_CONNECTABLE(address),
                    NULL,
                    &error);

                g_free(path);
                g_object_unref(client);
                g_object_unref(address);

                if (conn) {
                        discord_conn = conn;
                        discord_out = g_io_stream_get_output_stream(G_IO_STREAM(conn));
                        discord_available = TRUE;

                        // Immediately handshake on connect
                        char escaped_client_id[256];
                        char handshake[512];
                        json_escape(DISCORD_CLIENT_ID, escaped_client_id, sizeof(escaped_client_id));
                        snprintf(handshake, sizeof(handshake),
                                 "{\"v\":1,\"client_id\":\"%s\"}", escaped_client_id);
                        send_frame(DISCORD_OP_HANDSHAKE, handshake);

                        return TRUE;
                }

                if (error)
                        g_error_free(error);
        }
#endif

        discord_available = FALSE;
        return FALSE;
}

// Initialize Discord RPC (call at startup)
void discord_rpc_init(void)
{
        try_connect_socket();
}

void discord_rpc_shutdown(void)
{
        if (!discord_available)
                return;

        // Clear any activity on Discord before disconnecting
        discord_rpc_clear();

        // Close the GIO stream connection
        if (discord_conn) {
                g_io_stream_close(G_IO_STREAM(discord_conn), NULL, NULL);
                g_object_unref(discord_conn);
                discord_conn = NULL;
                discord_out = NULL;
        }

        discord_available = FALSE;
}

// Clear activity (pause)
void discord_rpc_clear(void)
{
        if (!discord_available) {
                try_connect_socket();
                if (!discord_available)
                        return;
        }

        char payload[256];
        snprintf(payload, sizeof(payload),
                 "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":%d},\"nonce\":\"kew_clear\"}",
                 getpid());

        send_frame(DISCORD_OP_FRAME, payload);
}

// Notify Discord of pause
void notify_discord_pause(void)
{
        discord_rpc_clear();
}

// Notify Discord of resume
void notify_discord_resume(SongData *song)
{
        if (!discord_available) {
                try_connect_socket();
                if (!discord_available || !song)
                        return;
        }

        notify_discord_switch(song);
}

// Notify Discord of song change or progress
void notify_discord_switch(SongData *song)
{
        if (!song || !song->metadata)
                return;

        if (!discord_available) {
                try_connect_socket();
                if (!discord_available)
                        return;
        }

        track_start_time = time(NULL);

        char escaped_title[512];
        char escaped_artist[512];
        char nonce[32];
        char payload[2048];

        const char *title = song->metadata->title;
        const char *artist = song->metadata->artist;

        json_escape(title, escaped_title, sizeof(escaped_title));
        json_escape(artist, escaped_artist, sizeof(escaped_artist));

        snprintf(nonce, sizeof(nonce), "%lld", (long long)g_get_monotonic_time());

        snprintf(payload, sizeof(payload),
                 "{"
                 "\"cmd\":\"SET_ACTIVITY\","
                 "\"args\":{"
                 "\"pid\":%d,"
                 "\"activity\":{"
                 "\"details\":\"%s\","
                 "\"state\":\"%s\","
                 "\"timestamps\":{"
                 "\"start\":%lld"
                 "},"
                 "\"assets\":{"
                 "\"large_image\":\"kew_logo\","
                 "\"large_text\":\"kew music player\""
                 "}"
                 "}"
                 "},"
                 "\"nonce\":\"%s\""
                 "}",
                 getpid(),
                 escaped_title,
                 escaped_artist,
                 (long long)track_start_time,
                 nonce);

        send_frame(DISCORD_OP_FRAME, payload);
}
