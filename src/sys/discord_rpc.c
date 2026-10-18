/**
 * @file discord_rpc.c
 * @brief Integration that shows kew status in discord.
 *
 * Implements a discord RPC integration,

 */
#include "discord_rpc.h"
#include "common/appstate.h"
#include <gio/gio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <gio/gwin32outputstream.h>
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

#define DISCORD_CLIENT_ID "1472901833633693696"
#define DISCORD_RECONNECT_INTERVAL_US (5 * G_USEC_PER_SEC)
#define DISCORD_MIN_UPDATE_INTERVAL_US (15 * G_USEC_PER_SEC)
#define DISCORD_OP_HANDSHAKE 0
#define DISCORD_OP_FRAME 1
#define DISCORD_SEEK_TOLERANCE_S 2

#ifndef _WIN32
static GSocketConnection *discord_conn = NULL; // Unix only. owns the stream
#else
static HANDLE discord_pipe = INVALID_HANDLE_VALUE; // Windows only; raw handle for peek/read
#endif
static GOutputStream *discord_out = NULL; // Unix: borrowed from discord_conn. Windows: owned.
static gboolean discord_available = FALSE;
static time_t track_start_time = 0;
static char last_track_key[600] = "";
static gint64 next_connect_us = 0;

static gint64 last_send_us = 0;
static char pending_payload[4096];
static gboolean has_pending = FALSE;

static void json_escape(const char *src, char *dst, size_t size)
{
        if (!dst || size == 0)
                return;

        size_t j = 0;

        if (!src) {
                dst[0] = '\0';
                return;
        }

        for (size_t i = 0; src[i]; i++) {
                unsigned char c = (unsigned char)src[i];

                if (j + 6 >= size)
                        break;

                switch (c) {
                case '"':  dst[j++] = '\\'; dst[j++] = '"';  break;
                case '\\': dst[j++] = '\\'; dst[j++] = '\\'; break;
                case '\n': dst[j++] = '\\'; dst[j++] = 'n';  break;
                case '\r': dst[j++] = '\\'; dst[j++] = 'r';  break;
                case '\t': dst[j++] = '\\'; dst[j++] = 't';  break;
                case '\b': dst[j++] = '\\'; dst[j++] = 'b';  break;
                case '\f': dst[j++] = '\\'; dst[j++] = 'f';  break;
                default:
                        if (c < 0x20)
                                j += (size_t)snprintf(dst + j, size - j, "\\u%04x", c);
                        else
                                dst[j++] = (char)c;
                        break;
                }
        }

        dst[j] = '\0';
}

static const char *or_default(const char *s, const char *fallback)
{
        return (s && s[0]) ? s : fallback;
}

static void disconnect(void)
{
#ifdef _WIN32
        if (discord_out) {
                g_output_stream_close(discord_out, NULL, NULL);
                g_object_unref(discord_out);
                discord_out = NULL;
        }
        discord_pipe = INVALID_HANDLE_VALUE;
#else
        if (discord_conn) {
                g_io_stream_close(G_IO_STREAM(discord_conn), NULL, NULL);
                g_clear_object(&discord_conn);
        }
        discord_out = NULL;
#endif
        discord_available = FALSE
        ;
        last_track_key[0] = '\0';
}

static void drain_responses(void)
{
        if (!discord_available)
                return;

        char buf[1024];

#ifdef _WIN32
        if (discord_pipe == INVALID_HANDLE_VALUE)
                return;

        for (;;) {
                DWORD avail = 0;
                if (!PeekNamedPipe(discord_pipe, NULL, 0, NULL, &avail, NULL)) {
                        disconnect();
                        return;
                }
                if (avail == 0)
                        return;

                DWORD got = 0;
                DWORD want = avail < sizeof(buf) ? avail : (DWORD)sizeof(buf);
                if (!ReadFile(discord_pipe, buf, want, &got, NULL) || got == 0) {
                        disconnect();
                        return;
                }
        }
#else
        if (!discord_conn)
                return;

        GSocket *sock = g_socket_connection_get_socket(discord_conn);

        for (;;) {
                if (!(g_socket_condition_check(sock, G_IO_IN) & G_IO_IN))
                        return;

                gssize avail = g_socket_get_available_bytes(sock);
                if (avail <= 0) {
                        disconnect();
                        return;
                }

                gsize want = (gsize)avail < sizeof(buf) ? (gsize)avail : sizeof(buf);
                GError *error = NULL;
                gssize got = g_socket_receive(sock, buf, want, NULL, &error);
                if (error)
                        g_error_free(error);
                if (got <= 0) {
                        disconnect();
                        return;
                }
        }
#endif
}

// Send a frame over IPC
static void send_frame(int opcode, const char *json)
{
        if (!discord_available || !discord_out || !json)
                return;

        size_t len = strlen(json);

        guint8 *frame = g_malloc(8 + len);
        guint32 header[2];
        header[0] = GUINT32_TO_LE((guint32)opcode);
        header[1] = GUINT32_TO_LE((guint32)len);
        memcpy(frame, header, 8);
        memcpy(frame + 8, json, len);

        gsize written = 0;
        GError *error = NULL;

        if (!g_output_stream_write_all(discord_out, frame, 8 + len, &written, NULL, &error))
                disconnect();

        if (error)
                g_error_free(error);

        g_free(frame);
}

static void send_handshake(void)
{
        char handshake[512];
        snprintf(handshake, sizeof(handshake),
                 "{\"v\":1,\"client_id\":\"%s\"}", DISCORD_CLIENT_ID);
        send_frame(DISCORD_OP_HANDSHAKE, handshake);
}

static void send_activity(const char *payload, gboolean force)
{
        gint64 now = g_get_monotonic_time();

        if (!force && last_send_us != 0 &&
            now - last_send_us < DISCORD_MIN_UPDATE_INTERVAL_US) {
                snprintf(pending_payload, sizeof(pending_payload), "%s", payload);
                has_pending = TRUE;
                return;
        }

        has_pending = FALSE;
        send_frame(DISCORD_OP_FRAME, payload);
        last_send_us = now;
}

// Same lookup order the official discord-rpc library uses.
static const char *discord_ipc_dir(void)
{
        const char *vars[] = {"XDG_RUNTIME_DIR", "TMPDIR", "TMP", "TEMP"};

        for (size_t i = 0; i < G_N_ELEMENTS(vars); i++) {
                const char *v = g_getenv(vars[i]);
                if (v && v[0])
                        return v;
        }
        return "/tmp";
}

static gboolean try_connect_socket_now(void)
{
        if (discord_available && discord_out)
                return TRUE;

#ifdef _WIN32
        for (int i = 0; i < 10; i++) {
                char path[64];
                snprintf(path, sizeof(path), "\\\\.\\pipe\\discord-ipc-%d", i);

                HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
                                       0, NULL, OPEN_EXISTING, 0, NULL);
                if (h == INVALID_HANDLE_VALUE)
                        continue;

                discord_pipe = h;
                discord_out = g_win32_output_stream_new(h, TRUE);
                discord_available = TRUE;

                send_handshake();
                return discord_available;
        }

        discord_available = FALSE;
        return FALSE;

#elif defined(__linux__) || defined(__FreeBSD__) || defined(__APPLE__)

        const char *runtime = discord_ipc_dir();

        if (!runtime) {
                discord_available = FALSE;
                return FALSE;
        }

        for (int i = 0; i < 10; i++) {
                gchar *path = g_strdup_printf("%s/discord-ipc-%d", runtime, i);
                GSocketClient *client = g_socket_client_new();
                GError *error = NULL;

                GSocketAddress *address = g_unix_socket_address_new(path);
                GSocketConnection *conn = g_socket_client_connect(
                    client, G_SOCKET_CONNECTABLE(address), NULL, &error);

                g_free(path);
                g_object_unref(client);
                g_object_unref(address);

                if (error)
                        g_error_free(error);

                if (conn) {
                        discord_conn = conn;
                        discord_out = g_io_stream_get_output_stream(G_IO_STREAM(conn));
                        discord_available = TRUE;

                        send_handshake();

                        return discord_available;
                }
        }

#endif

        discord_available = FALSE;
        return FALSE;
}

static gboolean try_connect_socket(void)
{
        if (discord_available && discord_out)
                return TRUE;

        gint64 now = g_get_monotonic_time();
        if (now < next_connect_us)
                return FALSE; // too soon after the last failure

        if (try_connect_socket_now())
                return TRUE;

        next_connect_us = now + DISCORD_RECONNECT_INTERVAL_US;
        return FALSE;
}

void discord_rpc_tick(void)
{
        if (!has_pending)
                return;

        if (g_get_monotonic_time() - last_send_us < DISCORD_MIN_UPDATE_INTERVAL_US)
                return;

        if (!discord_available) {
                try_connect_socket();
                if (!discord_available)
                        return;
        }

        drain_responses();
        if (!discord_available)
                return;

        has_pending = FALSE;
        send_frame(DISCORD_OP_FRAME, pending_payload);
        last_send_us = g_get_monotonic_time();
}

// Initialize Discord RPC (call at startup)
void discord_rpc_init(void)
{
        Model *model = get_model();

        if (model->state.settings.discordRPCEnabled)
                try_connect_socket();
}

static void clear_activity(gboolean force)
{
        last_track_key[0] = '\0'; // resuming must send a fresh update
        has_pending = FALSE;      // a queued update is now obsolete

        if (!discord_available)
                return;

        drain_responses();
        if (!discord_available)
                return;

        char payload[256];
        snprintf(payload, sizeof(payload),
                 "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":%d},\"nonce\":\"kew_clear\"}",
                 (int)getpid());

        send_activity(payload, force);
}

void discord_rpc_clear(void)
{
        clear_activity(FALSE);
}

void discord_rpc_shutdown(void)
{
        if (!discord_available)
                return;

        clear_activity(TRUE); // bypass the limiter so the presence really disappears
        disconnect();
}

// Notify Discord of pause
void notify_discord_pause(void)
{
        discord_rpc_clear();
}

#define DISCORD_FIELD_MAX 128

static void discord_field(const char *src, char *dst, size_t dst_size)
{
        size_t full = src ? strlen(src) : 0;
        size_t len = full > DISCORD_FIELD_MAX ? DISCORD_FIELD_MAX : full;

        if (len < full) {
                // Back up if we landed in the middle of a multi-byte character
                while (len > 0 && ((unsigned char)src[len] & 0xC0) == 0x80)
                        len--;
        }

        if (len > 0)
                memcpy(dst, src, len);

        while (len < 2 && len + 1 < dst_size)
                dst[len++] = ' ';

        dst[len] = '\0';
}

// Notify Discord of player state changes
void notify_discord_update(SongData *song, time_t elapsed_seconds, time_t song_duration)
{
        if (!song || !song->metadata)
                return;

        char track_key[600];
        snprintf(track_key, sizeof(track_key), "%s\n%s\n%s\n%lld",
                 or_default(song->metadata->title, ""),
                 or_default(song->metadata->artist, ""),
                 or_default(song->metadata->album, ""),
                 (long long)song_duration);

        time_t new_start = time(NULL) - elapsed_seconds;

        if (strcmp(track_key, last_track_key) == 0 &&
            llabs((long long)(new_start - track_start_time)) <= DISCORD_SEEK_TOLERANCE_S)
                return; // same track

        if (!discord_available) {
                try_connect_socket();
                if (!discord_available)
                        return;
        }

        drain_responses();
        if (!discord_available) {
                try_connect_socket();
                if (!discord_available)
                        return;
        }

        track_start_time = time(NULL) - elapsed_seconds;

        char title[132], artist[132], album[132];
        char escaped_title[800];
        char escaped_artist[800];
        char escaped_album[800];
        char nonce[32];
        char payload[4096];

        discord_field(or_default(song->metadata->title, "Unknown title"), title, sizeof(title));
        discord_field(or_default(song->metadata->artist, "Unknown artist"), artist, sizeof(artist));
        discord_field(or_default(song->metadata->album, "Unknown album"), album, sizeof(album));

        json_escape(title, escaped_title, sizeof(escaped_title));
        json_escape(artist, escaped_artist, sizeof(escaped_artist));
        json_escape(album, escaped_album, sizeof(escaped_album));

        snprintf(nonce, sizeof(nonce), "%lld", (long long)g_get_monotonic_time());

        if (elapsed_seconds < 0)
                elapsed_seconds = 0;
        track_start_time = time(NULL) - elapsed_seconds;

        char timestamps[96];
        if (song_duration > 0)
                snprintf(timestamps, sizeof(timestamps),
                         "{\"start\":%lld,\"end\":%lld}",
                         (long long)track_start_time,
                         (long long)(track_start_time + song_duration));
        else
                snprintf(timestamps, sizeof(timestamps),
                         "{\"start\":%lld}", (long long)track_start_time);

        int n = snprintf(payload, sizeof(payload),
                 "{"
                 "\"cmd\":\"SET_ACTIVITY\","
                 "\"args\":{"
                 "\"pid\":%d,"
                 "\"activity\":{"
                 "\"type\":2,"
                 "\"name\":\"%s\","
                 "\"details\":\"%s\","
                 "\"state\":\"%s\","
                 "\"timestamps\":%s,"
                 "\"assets\":{"
                 "\"large_image\":\"kew_logo\","
                 "\"large_text\":\"kew music player\""
                 "}"
                 "}"
                 "},"
                 "\"nonce\":\"%s\""
                 "}",
                 (int)getpid(),
                 escaped_artist,
                 escaped_title,
                 escaped_album,
                 timestamps,
                 nonce);

        if (n < 0 || n >= (int)sizeof(payload))
                return;

        snprintf(last_track_key, sizeof(last_track_key), "%s", track_key);
        send_activity(payload, FALSE);
}
