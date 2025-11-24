/**
 * @file chroma.c
 * @brief To handle running Chroma in a thread and printing its output
 *
 * Contains functions to start, stop and print a chroma frame.
 */

#include "chroma.h"

#include "data/img_func.h"
#include "utils/term.h"
#include "utils/utils.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
        char *frame;
        pthread_mutex_t lock;
        int height;
        int width;
        bool running;
        pthread_t thread;
        int preset;
        size_t frame_capacity;
} Chroma;

Chroma g_viz = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .height = 0,
    .width = 0,
    .running = false,
    .preset = 0,
};

#define CHROMA_MAX_BUF (512 * 1024)

#define MAX_HEIGHT 4000

#define MAX_PRESET 11

volatile int chroma_new_frame = 0;

static int centered_indent = 0;

static bool chroma_started;

void chroma_set_next_preset(int height)
{
        g_viz.preset++;

        if (g_viz.preset == 22)
                g_viz.preset = 0;

        if (g_viz.preset % MAX_PRESET == 0) {
                chroma_stop();
        } else {
                chroma_stop();
                chroma_start(height);
        }
}

int calc_chroma_width(int height)
{
        TermSize ts;
        tty_init();

        get_tty_size(&ts);

        int cell_w = (ts.width_pixels > 0 && ts.width_cells > 0) ? ts.width_pixels / ts.width_cells : 8;
        int cell_h = (ts.height_pixels > 0 && ts.height_cells > 0) ? ts.height_pixels / ts.height_cells : 16;
        float aspect = (float)cell_h / (float)cell_w;

        float aspect_ratio_correction = aspect;
        int corrected_width = (int)(height * aspect_ratio_correction) - 1;
        if (corrected_width < 0)
                corrected_width = 0;
        if (corrected_width > ts.width_cells)
                corrected_width = ts.width_cells;

        centered_indent = (ts.width_cells - corrected_width) / 2;

        int width = (int)(height * aspect);
        if (width < 1)
                width = 1;

        return width;
}

// FIXME: Enable Chroma
// static void *chroma_thread(void *arg)
// {
//         int height = *(int *)arg;
//         free(arg); // Caller must malloc the int

//         if (height <= 0)
//                 height = 1;
//         if (height > MAX_HEIGHT)
//                 height = MAX_HEIGHT;

//         int synced = 0;

//         g_viz.frame_capacity = CHROMA_MAX_BUF;
//         g_viz.frame = malloc(g_viz.frame_capacity);
//         if (!g_viz.frame) {
//                 exit(1);
//         }

//         g_viz.frame[0] = '\0';

//         while (g_viz.running) {

//                 pthread_mutex_lock(&g_viz.lock);
//                 g_viz.height = height;
//                 g_viz.width = calc_chroma_width(height);
//                 pthread_mutex_unlock(&g_viz.lock);

//                 char cmd[256];
//                 int n = snprintf(cmd, sizeof(cmd),
//                                  //"chroma --stream %dx%d --config /home/ravachol/Documents/chroma/examples/%d.toml --fps 30", width, height, g_viz.preset);
//                                  "chroma --stream %dx%d --fps 30", g_viz.width, g_viz.height);

//                 if (n < 0 || n >= (int)sizeof(cmd)) {
//                         // Truncated or error; skip this iteration
//                         usleep(100000);
//                         continue;
//                 }

//                 FILE *fp = popen(cmd, "r");
//                 if (!fp) {
//                         usleep(100000);
//                         continue;
//                 }

//                 size_t pos = 0;
//                 char *buf = malloc(CHROMA_MAX_BUF);

//                 if (!buf) {
//                         pclose(fp);
//                         usleep(100000);
//                         continue;
//                 }

//                 int nl_streak = 0;

//                 while (g_viz.running) {
//                         ssize_t r = read(fileno(fp), buf + pos, CHROMA_MAX_BUF - pos);
//                         if (r <= 0)
//                                 break; // EOF or error

//                         size_t end = pos + r; // total valid bytes in buf
//                         size_t start = 0;     // start of unprocessed bytes

//                         for (size_t i = pos; i < end; i++) {
//                                 if (buf[i] == '\n') {
//                                         nl_streak++;
//                                 } else {
//                                         nl_streak = 0;
//                                 }

//                                 if (nl_streak == 2) {
//                                         size_t frame_len = i - start + 1;

//                                         if (!synced) {
//                                                 synced = 1;
//                                                 start = i + 1;
//                                                 nl_streak = 0;
//                                                 continue;
//                                         }

//                                         if (frame_len < CHROMA_MAX_BUF && pthread_mutex_trylock(&g_viz.lock) == 0) {
//                                                 memcpy(g_viz.frame, buf + start, frame_len);
//                                                 g_viz.frame[frame_len] = '\0';
//                                                 chroma_new_frame = 1;
//                                                 pthread_mutex_unlock(&g_viz.lock);
//                                         }

//                                         start = i + 1; // move past this complete frame
//                                         nl_streak = 0;
//                                 }
//                         }

//                         // Move leftover bytes to the start of buffer for next read
//                         pos = end - start;
//                         if (pos > 0) {
//                                 memmove(buf, buf + start, pos);
//                         } else {
//                                 pos = 0;
//                         }

//                         // If buffer overflows, skip frame to avoid corruption
//                         if (pos >= CHROMA_MAX_BUF) {
//                                 pos = 0;
//                                 nl_streak = 0;
//                                 synced = 0;
//                         }
//                 }

//                 if (buf)
//                         free(buf);
//                 pclose(fp);
//         }

//         return NULL;
// }

void chroma_start(int height)
{
        // FIXME: Enable Chroma
        (void)height;
//         if (g_viz.running)
//                 return;
//         g_viz.running = 1;

//         int *arg = malloc(sizeof(int));
//         *arg = height;

//         pthread_create(&g_viz.thread, NULL, chroma_thread, arg);

//         chroma_started = true;
}

void chroma_stop()
{
        if (!g_viz.running)
                return;

        g_viz.running = 0;                // Tell thread to exit
        pthread_join(g_viz.thread, NULL); // Wait until it finishes

        pthread_mutex_lock(&g_viz.lock);
        if (g_viz.frame) {
                free(g_viz.frame);
                g_viz.frame = NULL;
        }
        pthread_mutex_unlock(&g_viz.lock);

        chroma_started = false;
}

void chroma_print_frame(int row, int col, bool centered)
{
        if (!chroma_new_frame)
                return;

        char *tmp_buf = NULL;
        size_t frame_len = 0;

        pthread_mutex_lock(&g_viz.lock);
        if (g_viz.frame) {
                frame_len = strlen(g_viz.frame);
                tmp_buf = malloc(frame_len + 1);
                if (tmp_buf) {
                        memcpy(tmp_buf, g_viz.frame, frame_len + 1);
                        chroma_new_frame = 0; // Mark as consumed
                }
        }
        pthread_mutex_unlock(&g_viz.lock);

        char *p = tmp_buf;

        if (!p)
                return;

        if (centered)
                col = centered_indent;

        printf("\033[%d;%dH", row, col + 1);

        int row_pos = row;

        while (*p) {

                const char *start = p;

                while (*p && *p != '\n')
                        p++;

                if (*p == '\n')
                {
                        printf("\033[%d;%dH", row_pos, col + 1);
                        fwrite(start, 1, p - start, stdout);
                        p++;
                        row_pos++;
                }
                if (*p == '\n')
                {
                        row_pos = row;
                        p++;
                }

        }

        fflush(stdout);

        if (tmp_buf) {
                free(tmp_buf);
                tmp_buf = NULL;
        }
}

bool chroma_is_installed(void)
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
                snprintf(fullpath, sizeof(fullpath), "%s/chroma", dir);
                if (access(fullpath, X_OK) == 0) {
                        free(p);
                        return true;
                }
                dir = strtok(NULL, ":");
        }

        free(p);
        return false;
}

bool chroma_is_started(void)
{
        return chroma_started;
}

int chroma_get_current_preset(void)
{
        return g_viz.preset;
}

void chroma_set_current_preset(int preset)
{
        g_viz.preset = preset;
}
