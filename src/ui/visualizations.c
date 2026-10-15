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

volatile int visualizations_new_frame = 0;

static int centered_indent = 0;

int chroma_max_prests = 25;

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

static int set_chroma_cmd_string(char *cmd, int cmd_size, char *preset_arg)
{

        AppSettings *settings = get_app_settings();
        int n;

        if (settings->chromaPath[0] == '\0') {
                if (settings->visualizationsDevice[0] == '\0')
                        n = snprintf(cmd, cmd_size,
                                     "%s --stream %dx%d --preset %s --preset-interval 30",
                                     g_viz.provider, g_viz.width, g_viz.height, preset_arg);
                else
                        n = snprintf(cmd, cmd_size,
                                     "%s --stream %dx%d --preset %s --audio-device \"%s\"",
                                     g_viz.provider, g_viz.width, g_viz.height, preset_arg,
                                     settings->visualizationsDevice);
        } else {
                if (settings->visualizationsDevice[0] == '\0')
                        n = snprintf(cmd, cmd_size,
                                     "%s --stream %dx%d -c \"%s\"",
                                     g_viz.provider, g_viz.width, g_viz.height, settings->chromaPath);
                else
                        n = snprintf(cmd, cmd_size,
                                     "%s --stream %dx%d -c \"%s\" --presets --audio-device \"%s\"",
                                     g_viz.provider, g_viz.width, g_viz.height, settings->chromaPath,
                                     settings->visualizationsDevice);
        }

        return n;
}

static int set_generic_cmd_string(char *cmd, int cmd_size)
{

        Model *model = get_model();
        AppSettings *settings = get_app_settings();
        int n;

        TermSize ts;
        get_tty_size(&ts);
        int cw = ts.width_pixels / ts.cols;
        int ch = ts.height_pixels / ts.rows;

        char extra[96];
        snprintf(extra, sizeof(extra), " --mode %s --cell-size %dx%d",
                 model->state.settings.visualizationsMode == VIZ_SIXELS ? "sixels" : "ascii", cw, ch);

        g_viz.mode = model->state.settings.visualizationsMode;

        if (settings->visualizationsPath[0] == '\0') {
                if (settings->visualizationsDevice[0] == '\0')
                        n = snprintf(cmd, cmd_size,
                                     "%s --stream %dx%d %s",
                                     g_viz.provider, g_viz.width, g_viz.height, extra);
                else
                        n = snprintf(cmd, cmd_size,
                                     "%s --stream %dx%d --audio-device \"%s\" %s",
                                     g_viz.provider, g_viz.width, g_viz.height, settings->visualizationsDevice, extra);
        } else {
                if (settings->visualizationsDevice[0] == '\0')
                        n = snprintf(cmd, cmd_size,
                                     "%s --stream %dx%d --presets \"%s\" %s",
                                     g_viz.provider, g_viz.width, g_viz.height, settings->visualizationsPath, extra);
                else
                        n = snprintf(cmd, cmd_size,
                                     "%s --stream %dx%d --presets \"%s\" --audio-device \"%s\" %s",
                                     g_viz.provider, g_viz.width, g_viz.height, settings->visualizationsPath,
                                     settings->visualizationsDevice, extra);
        }

        return n;
}

static void *visualizer_thread(void *arg)
{
        VisualizationsThreadArgs *args = (VisualizationsThreadArgs *)arg;
        int height = args->height;
        int width = args->width;

        if (args->provider[0] == '\0')
                return NULL;

        snprintf(g_viz.provider, sizeof(g_viz.provider), args->provider);

        free(arg); // Caller must malloc the `VisualizerThreadArgs`

        if (height <= 0)
                height = 1;
        if (height > MAX_HEIGHT)
                height = MAX_HEIGHT;

        int synced = 0;

        g_viz.frame_capacity = VISUALIZATIONS_MAX_BUF;
        g_viz.frame = malloc(g_viz.frame_capacity);
        if (!g_viz.frame) {
                quit();
        }

        g_viz.frame[0] = '\0';

        while (g_viz.running) {

                pthread_mutex_lock(&g_viz.lock);
                g_viz.height = height;
                // g_viz.width = calc_visualizations_width(height);
                g_viz.width = width;
                pthread_mutex_unlock(&g_viz.lock);

                int cmd_size = 512;
                char cmd[1024];
                int n;

                char preset_arg[32];
                if (g_viz.preset == -1)
                        snprintf(preset_arg, sizeof(preset_arg), "random");
                else
                        snprintf(preset_arg, sizeof(preset_arg), "%d", g_viz.preset);

                if (strcmp(g_viz.provider, "chroma") == 0) {
                        g_viz.mode = VIZ_ASCII;
                        n = set_chroma_cmd_string(cmd, cmd_size, preset_arg);
                } else {
                        n = set_generic_cmd_string(cmd, cmd_size);
                }

                //"chroma --stream %dx%d --fps 30", g_viz.width, g_viz.height);

                //n = snprintf(cmd, sizeof(cmd),
                //      "chroma --stream %dx%d --audio-device PipeWire\\ Sound\\ Server -c %s ", g_viz.width, g_viz.height, settings->visualizerPath);

                if (n < 0 || n >= (int)sizeof(cmd)) {
                        // Truncated or error; skip this iteration
                        usleep(1000);
                        continue;
                }

                FILE *fp = popen(cmd, "r");
                if (!fp) {
                        usleep(1000);
                        continue;
                }

                size_t pos = 0;
                char *buf = malloc(VISUALIZATIONS_MAX_BUF);

                if (!buf) {
                        pclose(fp);
                        usleep(1000);
                        continue;
                }

                int nl_streak = 0;

                while (g_viz.running) {
                        ssize_t r = read(fileno(fp), buf + pos, VISUALIZATIONS_MAX_BUF - pos);
                        if (r <= 0)
                                break; // EOF or error

                        size_t end = pos + r; // total valid bytes in buf
                        size_t start = 0;     // start of unprocessed bytes

                        for (size_t i = pos; i < end; i++) {
                                if (buf[i] == '\n') {
                                        nl_streak++;
                                } else {
                                        nl_streak = 0;
                                }

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
                pclose(fp);
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

        }
        else {
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
        g_viz.running = 1;

        visualizations_set_current_preset(visualizations_get_current_preset());

        VisualizationsThreadArgs *arg = malloc(sizeof(VisualizationsThreadArgs));
        if (!arg)
                return;

        Model *model = get_model();

        arg->height = height;
        arg->width = width;
        snprintf(arg->provider, sizeof(arg->provider), model->state.ui.visualizations_provider);

        pthread_create(&g_viz.thread, NULL, visualizer_thread, arg);

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
        if (preset == 25)
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
                                printf("\033[%d;%dH", row_pos, col + 1);
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

bool visualizations_are_installed(const char *appname)
{
        char path[MAX_PATH];

        return SearchPathA(
                   NULL,
                   appname,
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
