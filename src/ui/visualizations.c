/**
 * @file cvisualizer.c
 * @brief To handle running visualizations in a thread and printing its output
 *
 * Contains functions to start, stop and print a visualization frame.
 */

#include "visualizations.h"

#include "common/appstate.h"
#include "common/common.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef _WIN32
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#else
#include <fcntl.h>
#include <io.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#endif

#define VIZ_MAX_ARGS 32

extern char **environ;

typedef struct {
        const char *argv[VIZ_MAX_ARGS];
        int argc;
        char stream[32];
        char preset[32];
        char cellsize[32];
} VizCmd;

enum viz_type {
        VIZ_ASCII = 0,
        VIZ_SIXELS = 1
};

typedef struct {
        int height;
        int width;
        char provider[256];
} VisualizationsThreadArgs;

typedef struct {
        char *frame;
        pthread_mutex_t lock;
        int height;
        int width;
        bool running;
        pthread_t thread;
        int preset;
        char provider[256];
        int mode;
        size_t frame_capacity;
} Visualizations;

Visualizations g_viz = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .height = 0,
    .width = 0,
    .running = false,
    .preset = -1,
    .provider = "pmstream",
    .mode = VIZ_ASCII};

#define VISUALIZATIONS_MAX_BUF (8 * 1024 * 1024)
#define MAX_HEIGHT 4000

#ifdef _WIN32
typedef HANDLE viz_proc_t;
#define VIZ_PROC_INVALID NULL
#else
typedef pid_t viz_proc_t;
#define VIZ_PROC_INVALID ((pid_t) - 1)
#endif

volatile int visualizations_new_frame = 0;

static int centered_indent = 0;

int chroma_max_prests = 25;

static void viz_push(VizCmd *c, const char *s)
{
        if (c->argc < VIZ_MAX_ARGS - 1)
                c->argv[c->argc++] = s;
        c->argv[c->argc] = NULL;
}

static void build_chroma_cmd(VizCmd *c)
{
        Model *model = get_model();
        AppSettings *settings = get_app_settings();
        bool has_device = settings->visualizationsDevice[0] != '\0';

        snprintf(c->stream, sizeof(c->stream), "%dx%d", g_viz.width, g_viz.height);

        if (g_viz.preset == -1)
                snprintf(c->preset, sizeof(c->preset), "random");
        else
                snprintf(c->preset, sizeof(c->preset), "%d", g_viz.preset);

        viz_push(c, g_viz.provider);
        viz_push(c, "--stream");
        viz_push(c, c->stream);

        if (model->state.settings.visualizationsFps > 0) {
                viz_push(c, "--fps");
                char fps[6];
                snprintf(fps, sizeof(fps), "%d", model->state.settings.visualizationsFps);
                viz_push(c, fps);
        }

        if (settings->chromaPath[0] == '\0') {
                viz_push(c, "--preset");
                viz_push(c, c->preset);
                if (has_device) {
                        viz_push(c, "--audio-device");
                        viz_push(c, settings->visualizationsDevice);
                } else {
                        viz_push(c, "--preset-interval");
                        viz_push(c, "30");
                }
        } else {
                viz_push(c, "-c");
                viz_push(c, settings->chromaPath);
                if (has_device) {
                        viz_push(c, "--presets");
                        viz_push(c, "--audio-device");
                        viz_push(c, settings->visualizationsDevice);
                }
        }
}

static void build_generic_cmd(VizCmd *c)
{
        Model *model = get_model();
        AppSettings *settings = get_app_settings();

        TermSize ts;
        get_tty_size(&ts);
        int cw = (ts.width_pixels > 0 && ts.cols > 0) ? ts.width_pixels / ts.cols : 8;
        int ch = (ts.height_pixels > 0 && ts.rows > 0) ? ts.height_pixels / ts.rows : 16;

        g_viz.mode = model->state.settings.visualizationsMode;

        snprintf(c->stream, sizeof(c->stream), "%dx%d", g_viz.width, g_viz.height);
        snprintf(c->cellsize, sizeof(c->cellsize), "%dx%d", cw, ch);

        viz_push(c, g_viz.provider);
        viz_push(c, "--stream");
        viz_push(c, c->stream);

        if (settings->visualizationsPath[0] != '\0') {
                viz_push(c, "--presets");
                viz_push(c, settings->visualizationsPath);
        }
        if (settings->visualizationsDevice[0] != '\0') {
                viz_push(c, "--audio-device");
                viz_push(c, settings->visualizationsDevice);
        }

        if (model->state.settings.visualizationsShowPresetInfo)
                viz_push(c, "--sendinfo");

        if (model->state.settings.visualizationsFps > 0) {
                viz_push(c, "--fps");
                char fps[6];
                snprintf(fps, sizeof(fps), "%d", model->state.settings.visualizationsFps);
                viz_push(c, fps);
        }
        viz_push(c, "--mode");
        viz_push(c, g_viz.mode == VIZ_SIXELS ? "sixels" : "ascii");
        viz_push(c, "--cell-size");
        viz_push(c, c->cellsize);
}

#ifndef _WIN32
// Starts the child with its stdout connected to a pipe. Returns the pid
// (or -1) and puts the read end of the pipe in *out_fd. No shell is involved.
static pid_t viz_spawn(const char *const argv[], int *out_fd)
{
        int fds[2];
        if (pipe(fds) != 0)
                return -1;

        fcntl(fds[0], F_SETFD, FD_CLOEXEC); // don't leak the read end into later children

        posix_spawn_file_actions_t fa;
        posix_spawn_file_actions_init(&fa);
        posix_spawn_file_actions_adddup2(&fa, fds[1], STDOUT_FILENO);
        posix_spawn_file_actions_addclose(&fa, fds[1]);
        posix_spawn_file_actions_addopen(&fa, STDIN_FILENO, "/dev/null", O_RDONLY, 0);

        pid_t pid;
        int rc = posix_spawnp(&pid, argv[0], &fa, NULL, (char *const *)argv, environ);

        posix_spawn_file_actions_destroy(&fa);
        close(fds[1]);

        if (rc != 0) {
                close(fds[0]);
                return -1;
        }

        *out_fd = fds[0];
        return pid;
}

static void viz_stop(pid_t pid, int fd)
{
        close(fd);
        kill(pid, SIGTERM);

        for (int i = 0; i < 50; i++) { // up to ~1 second
                if (waitpid(pid, NULL, WNOHANG) != 0)
                        return; // reaped, or already gone
                usleep(20 * 1000);
        }

        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
}

#else

static char *viz_build_cmdline(const char *const argv[])
{
        size_t cap = 1;
        for (int i = 0; argv[i]; i++)
                cap += 2 * strlen(argv[i]) + 3;

        char *buf = malloc(cap);
        if (!buf)
                return NULL;

        char *p = buf;
        for (int i = 0; argv[i]; i++) {
                const char *a = argv[i];
                if (i > 0)
                        *p++ = ' ';

                if (*a && !strpbrk(a, " \t\n\v\"")) {
                        size_t n = strlen(a);
                        memcpy(p, a, n);
                        p += n;
                        continue;
                }

                *p++ = '"';
                for (;; a++) {
                        size_t bs = 0;
                        while (*a == '\\') {
                                a++;
                                bs++;
                        }
                        if (*a == '\0') {
                                // Backslashes before the closing quote must be doubled
                                for (size_t k = 0; k < bs * 2; k++)
                                        *p++ = '\\';
                                break;
                        } else if (*a == '"') {
                                for (size_t k = 0; k < bs * 2 + 1; k++)
                                        *p++ = '\\';
                                *p++ = '"';
                        } else {
                                for (size_t k = 0; k < bs; k++)
                                        *p++ = '\\';
                                *p++ = *a;
                        }
                }
                *p++ = '"';
        }
        *p = '\0';
        return buf;
}

static viz_proc_t viz_spawn(const char *const argv[], int *out_fd)
{
        viz_proc_t result = NULL;
        HANDLE rd = NULL, wr = NULL;
        HANDLE nul = INVALID_HANDLE_VALUE;
        PROCESS_INFORMATION pi = {0};
        STARTUPINFOA si = {0};
        SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
        int fd;

        char *cmd = viz_build_cmdline(argv);
        if (!cmd)
                return NULL;

        if (!CreatePipe(&rd, &wr, &sa, 0))
                goto done;

        // Don't leak the read end into the child
        if (!SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0))
                goto done;

        nul = CreateFileA("NUL", GENERIC_READ | GENERIC_WRITE,
                          FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                          OPEN_EXISTING, 0, NULL);
        if (nul == INVALID_HANDLE_VALUE)
                goto done;

        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = nul; // Like /dev/null
        si.hStdOutput = wr;
        si.hStdError = nul; // Keep child stderr from scribbling over the TUI

        if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                            NULL, NULL, &si, &pi))
                goto done;

        CloseHandle(pi.hThread);

        // The CRT fd now owns the read handle; close() will close it
        fd = _open_osfhandle((intptr_t)rd, _O_RDONLY | _O_BINARY);
        if (fd < 0) {
                TerminateProcess(pi.hProcess, 1);
                CloseHandle(pi.hProcess);
                goto done;
        }

        rd = NULL; // Ownership transferred
        *out_fd = fd;
        result = pi.hProcess;

done:
        if (wr)
                CloseHandle(wr);
        if (rd)
                CloseHandle(rd);
        if (nul != INVALID_HANDLE_VALUE)
                CloseHandle(nul);
        free(cmd);
        return result;
}

static void viz_stop(viz_proc_t proc, int fd)
{
        close(fd); // Similar to SIGTERM

        if (WaitForSingleObject(proc, 1000) == WAIT_TIMEOUT) {
                TerminateProcess(proc, 1); // the SIGKILL equivalent
                WaitForSingleObject(proc, INFINITE);
        }

        CloseHandle(proc);
}

#endif

int calc_visualizations_width(int height)
{
        TermSize ts;

        get_tty_size(&ts);

        int cell_w = (ts.width_pixels > 0 && ts.cols > 0) ? ts.width_pixels / ts.cols : 8;
        int cell_h = (ts.height_pixels > 0 && ts.rows > 0) ? ts.height_pixels / ts.rows : 16;
        float aspect = (float)cell_h / (float)cell_w;

        float aspect_ratio_correction = aspect;
        int corrected_width = (int)(height * aspect_ratio_correction) - 1;
        if (corrected_width < 0)
                corrected_width = 0;
        if (corrected_width > ts.cols)
                corrected_width = ts.cols;

        centered_indent = (ts.cols - corrected_width) / 2;

        int width = (int)(height * aspect);
        if (width < 1)
                width = 1;

        return width;
}

static void *visualizer_thread(void *arg)
{
        VisualizationsThreadArgs *args = (VisualizationsThreadArgs *)arg;
        int height = args->height;
        int width = args->width;

        snprintf(g_viz.provider, sizeof(g_viz.provider), "%s", args->provider);

        free(arg); // Caller must malloc the `VisualizerThreadArgs`

        if (g_viz.provider[0] == '\0')
                return NULL;

        if (height <= 0)
                height = 1;
        if (height > MAX_HEIGHT)
                height = MAX_HEIGHT;

        int synced = 0;
        int ok;

        pthread_mutex_lock(&g_viz.lock);
        if (!g_viz.frame) {
                g_viz.frame = malloc(VISUALIZATIONS_MAX_BUF);
                if (g_viz.frame)
                        g_viz.frame_capacity = VISUALIZATIONS_MAX_BUF;
        }
        ok = (g_viz.frame != NULL);
        if (ok) {
                g_viz.frame[0] = '\0';
                visualizations_new_frame = 0; // don't show a stale frame from the previous run
        }
        pthread_mutex_unlock(&g_viz.lock);

        if (!ok)
                quit();

        g_viz.frame[0] = '\0';

        while (g_viz.running) {

                pthread_mutex_lock(&g_viz.lock);
                g_viz.height = height;
                // g_viz.width = calc_visualizations_width(height);
                g_viz.width = width;
                pthread_mutex_unlock(&g_viz.lock);

                VizCmd cmd = {0};

                if (strcmp(g_viz.provider, "chroma") == 0) {
                        g_viz.mode = VIZ_ASCII;
                        build_chroma_cmd(&cmd);
                } else {
                        build_generic_cmd(&cmd);
                }

                int fd = -1;
                viz_proc_t pid = viz_spawn(cmd.argv, &fd);

                if (pid == VIZ_PROC_INVALID) {
                        usleep(200 * 1000);
                        continue;
                }

                size_t pos = 0;
                char *buf = malloc(VISUALIZATIONS_MAX_BUF);

                if (!buf) {
                        viz_stop(pid, fd);
                        usleep(200 * 1000);
                        continue;
                }

                int nl_streak = 0;

                while (g_viz.running) {

#ifdef _WIN32
                        DWORD avail = 0;
                        if (!PeekNamedPipe((HANDLE)_get_osfhandle(fd), NULL, 0,
                                           NULL, &avail, NULL))
                                break; // ERROR_BROKEN_PIPE: child exited
                        if (avail == 0) {
                                Sleep(10);
                                continue; // re-check g_viz.running
                        }
#else
                        struct pollfd pfd = {.fd = fd, .events = POLLIN};
                        int pr = poll(&pfd, 1, 100);
                        if (pr == 0)
                                continue;
                        if (pr < 0) {
                                if (errno == EINTR)
                                        continue;
                                break;
                        }
#endif
                        ssize_t r = read(fd, buf + pos, VISUALIZATIONS_MAX_BUF - pos);

                        if (r <= 0)
                                break; // EOF or error

                        size_t end = pos + r; // total valid bytes in buf
                        size_t start = 0;     // start of unprocessed bytes

                        for (size_t i = pos; i < end; i++) {
                                if (buf[i] == '\r')
                                        continue; // tolerate CRLF from Windows children
                                else if (buf[i] == '\n')
                                        nl_streak++;
                                else
                                        nl_streak = 0;

                                if (nl_streak == 2) {
                                        size_t frame_len = i - start + 1;

                                        if (!synced) {
                                                synced = 1;
                                                start = i + 1;
                                                nl_streak = 0;
                                                continue;
                                        }

                                        if (frame_len < VISUALIZATIONS_MAX_BUF && pthread_mutex_trylock(&g_viz.lock) == 0) {
                                                memcpy(g_viz.frame, buf + start, frame_len);
                                                g_viz.frame[frame_len] = '\0';
                                                visualizations_new_frame = 1;
                                                pthread_mutex_unlock(&g_viz.lock);
                                        }

                                        start = i + 1; // move past this complete frame
                                        nl_streak = 0;
                                }
                        }

                        // Move leftover bytes to the start of buffer for next read
                        pos = end - start;
                        if (pos > 0) {
                                memmove(buf, buf + start, pos);
                        } else {
                                pos = 0;
                        }

                        // If buffer overflows, skip frame to avoid corruption
                        if (pos >= VISUALIZATIONS_MAX_BUF) {
                                pos = 0;
                                nl_streak = 0;
                                synced = 0;
                        }
                }

                if (buf)
                        free(buf);
                viz_stop(pid, fd);

                if (g_viz.running)
                        usleep(200 * 1000); // don't respawn in a tight loop
        }

        return NULL;
}

int visualizations_get_current_preset(void)
{
        return g_viz.preset;
}

void visualizations_set_current_preset(int preset)
{
        g_viz.preset = preset;

        char error_message[ERROR_MESSAGE_LENGTH];

        if (strcmp(g_viz.provider, "chroma") == 0) {

                if (g_viz.preset == -1)
                        snprintf(error_message, sizeof(error_message), "Visualizations Preset Random");
                else
                        snprintf(error_message, sizeof(error_message), "Visualizations Preset %d", preset);

        } else {
                snprintf(error_message, sizeof(error_message), "Visualizations Switched Preset");
        }

        set_error_message(error_message);
}

bool visualizations_are_started(void)
{
        Model *model = get_model();
        return model->state.ui.visualizations_started;
}

void visualizations_start(int height, int width)
{
        if (g_viz.running)
                return;

        Model *model = get_model();

        if (model->state.ui.visualizations_provider[0] == '\0')
                return;

        VisualizationsThreadArgs *arg = malloc(sizeof(VisualizationsThreadArgs));
        if (!arg)
                return;

        arg->height = height;
        arg->width = width;
        snprintf(arg->provider, sizeof(arg->provider), "%s", model->state.ui.visualizations_provider);

        // Set provider here, before the thread exists and before the preset message.
        snprintf(g_viz.provider, sizeof(g_viz.provider), "%s", arg->provider);

        g_viz.running = 1;

        visualizations_set_current_preset(visualizations_get_current_preset());

        if (pthread_create(&g_viz.thread, NULL, visualizer_thread, arg) != 0) {
                g_viz.running = 0;
                free(arg);
                return;
        }

        model->state.settings.visualizationsEnabled = true;
        model->state.ui.visualizations_started = true;
        model->state.ui.visualizations_start_requested = false;
        model->state.ui.visualizations_height = height;
        model->state.ui.visualizations_width = width;
}

void visualizations_shutdown()
{
        if (!g_viz.running)
                return;

        Model *model = get_model();

        if (visualizations_are_started())

                model->state.settings.visualizationsPreset = visualizations_get_current_preset();
        else
                model->state.settings.visualizationsPreset = -1;

        g_viz.running = 0;                // Tell thread to exit
        pthread_join(g_viz.thread, NULL); // Wait until it finishes

        pthread_mutex_lock(&g_viz.lock);
        if (g_viz.frame) {
                free(g_viz.frame);
                g_viz.frame = NULL;
        }
        pthread_mutex_unlock(&g_viz.lock);

        model->state.ui.visualizations_started = false;
        model->state.ui.visualizations_start_requested = false;
}

void visualizations_start_with_preset(int preset)
{
        if (preset == chroma_max_prests)
                preset = -1;

        visualizations_set_current_preset(preset);

        AppState *state = get_app_state();
        state->settings.visualizationsPreset = g_viz.preset;

        visualizations_shutdown();
        visualizations_start(state->ui.visualizations_height, state->ui.visualizations_width);
}

void visualizations_set_next_preset(void)
{
        int preset = visualizations_get_current_preset() + 1;

        if (strcmp(g_viz.provider, "chroma") == 0 && preset == chroma_max_prests)
                preset = -1;

        visualizations_set_current_preset(preset);

        AppState *state = get_app_state();
        state->settings.visualizationsPreset = g_viz.preset;

        visualizations_shutdown();
        visualizations_start(state->ui.visualizations_height, state->ui.visualizations_width);
}

void visualizations_print_frame(int row, int col, int height, int width,
                                bool centered)
{
        if (!visualizations_are_started()) {
                visualizations_start(height, width);
                return;
        }

        char *tmp_buf = NULL;
        size_t frame_len = 0;

        pthread_mutex_lock(&g_viz.lock);

        if (visualizations_new_frame && g_viz.frame) {
                frame_len = strlen(g_viz.frame);
                tmp_buf = malloc(frame_len + 1);

                if (tmp_buf) {
                        memcpy(tmp_buf, g_viz.frame, frame_len + 1);
                        visualizations_new_frame = 0;
                }
        }

        pthread_mutex_unlock(&g_viz.lock);

        if (!tmp_buf)
                return;

        if (centered)
                col = centered_indent;

        if (g_viz.mode == VIZ_SIXELS) {
                /*
                 * SIXEL is one graphics sequence, not
                 * newline-separated text.
                 */
                size_t len = frame_len;

                /* Remove the trailing frame delimiter only. */
                while (len >= 2 &&
                       tmp_buf[len - 1] == '\n' &&
                       tmp_buf[len - 2] == '\n') {
                        len -= 2;
                }

                if (len > 0) {
                        printf("\033[%d;%dH", row, col);
                        fwrite(tmp_buf, 1, len, stdout);
                }
        } else {
                /*
                 * ASCII: print each line at the requested
                 * column, preserving the frame delimiter.
                 */
                const char *p = tmp_buf;
                int row_pos = row;

                while (*p) {

                        const char *start = p;

                        while (*p && *p != '\n')
                                p++;

                        if (*p == '\n') {
                                printf("\033[%d;%dH", row_pos, col);
                                fwrite(start, 1, p - start, stdout);
                                p++;
                                row_pos++;
                        }

                        if (*p == '\n') {
                                row_pos = row;
                                p++;
                        }
                }
        }

        fflush(stdout);
        free(tmp_buf);
}

#ifdef _WIN32

#include <windows.h>

bool visualizations_are_installed(char *appname){
        char path[MAX_PATH];
        char exe_name[MAX_PATH];

        snprintf(exe_name, sizeof(exe_name), "%s.exe", appname);

        return SearchPathA(
                NULL,
                exe_name,
                NULL,
                sizeof(path),
                path,
                NULL) != 0;
}

#else

bool visualizations_are_installed(char *appname)
{
        const char *path = getenv("PATH");
        if (!path)
                return false;

        char *p = strdup(path);
        if (!p)
                return false;

        char *dir = strtok(p, ":");
        char fullpath[512];

        while (dir) {
                snprintf(fullpath, sizeof(fullpath),
                         "%s/%s", dir, appname);

                if (access(fullpath, X_OK) == 0) {
                        free(p);
                        return true;
                }

                dir = strtok(NULL, ":");
        }

        free(p);
        return false;
}

#endif
