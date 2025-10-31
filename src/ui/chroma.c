#include "chroma.h"

#include "data/img_func.h" // your terminal size functions
#include "utils/term.h"
#include "utils/utils.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    char *frame;
    pthread_mutex_t lock;
    bool running;
    pthread_t thread;
    size_t frame_capacity;
} Chroma;

Chroma g_viz = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .running = false,
};

volatile int chroma_new_frame = 0;

static int centered_indent = 0;

static void *chroma_thread(void *arg)
{
    int height = *(int *)arg;
    free(arg);

    int synced = 0;

    while (g_viz.running) {

        TermSize ts;
        tty_init();
        get_tty_size(&ts);

        int cell_w = (ts.width_pixels > 0 && ts.width_cells > 0) ? ts.width_pixels / ts.width_cells : 8;
        int cell_h = (ts.height_pixels > 0 && ts.height_cells > 0) ? ts.height_pixels / ts.height_cells : 16;
        float aspect = (float)cell_h / (float)cell_w;

        float aspect_ratio_correction = (float)cell_h / (float)cell_w;
        unsigned int corrected_width = (int)(height * aspect_ratio_correction) - 1;

        // Calculate indentation to center the image
        centered_indent = ((ts.width_cells - corrected_width) / 2);

        if (height > MAX_HEIGHT) height = MAX_HEIGHT;
        int width = (int)(height * aspect);
        if (width < 1) width = 1;

        char cmd[128];
        snprintf(cmd, sizeof(cmd), "chroma --stream %dx%d --fps 30", width, height);

        FILE *fp = popen(cmd, "r");
        if (!fp) {
            usleep(100000);
            continue;
        }

        size_t cap = 128 * 1024;
        size_t pos = 0;
        char *buf = malloc(cap);
        if (!buf) {
            pclose(fp);
            usleep(100000);
            continue;
        }

        int nl_streak = 0;

        while (g_viz.running) {
            int c = fgetc(fp);
            if (c == EOF) break;

            // Grow buffer if needed
            if (pos + 1 >= cap) {
                cap *= 2;
                char *tmp = realloc(buf, cap);
                if (!tmp) {
                    // Abort this stream, drop frame
                    free(buf);
                    buf = NULL;
                    break;
                }
                buf = tmp;
            }

            buf[pos++] = (char)c;
            buf[pos] = '\0';

            // Detect end-of-frame
            if (c == '\n') {
                nl_streak++;
                if (nl_streak == 2) {
                    if (!synced) {
                        // Swallow first frame (sync)
                        pos = 0;
                        nl_streak = 0;
                        synced = 1;
                        continue;
                    }

                    // FULL FRAME READY
                    pthread_mutex_lock(&g_viz.lock);

                    if (!g_viz.frame || g_viz.frame_capacity < pos + 1) {
                        g_viz.frame = realloc(g_viz.frame, pos + 1);
                        g_viz.frame_capacity = pos + 1;
                    }

                    memcpy(g_viz.frame, buf, pos);
                    g_viz.frame[pos] = '\0';
                    chroma_new_frame = 1;

                    pthread_mutex_unlock(&g_viz.lock);

                    // Reset for next frame
                    pos = 0;
                    nl_streak = 0;
                }
            } else {
                nl_streak = 0;
            }
        }

        if (buf) free(buf);
        pclose(fp);
    }

    return NULL;
}

void chroma_start(int height)
{
    if (g_viz.running) return;
    g_viz.running = 1;

    int *arg = malloc(sizeof(int));
    *arg = height;

    pthread_create(&g_viz.thread, NULL, chroma_thread, arg);
}

void chroma_stop()
{
    if (!g_viz.running) return;

    g_viz.running = 0;                  // Tell thread to exit
    pthread_join(g_viz.thread, NULL);   // Wait until it finishes

    pthread_mutex_lock(&g_viz.lock);
    if (g_viz.frame) {                  // Free dynamic frame buffer
        free(g_viz.frame);
        g_viz.frame = NULL;
    }
    pthread_mutex_unlock(&g_viz.lock);
}

const char *chroma_get_frame()
{
        return g_viz.frame;
}

void print_chroma_frame(int row, int col, bool centered)
{
        if (!chroma_new_frame)
                return;

        const char *frame = chroma_get_frame();
        if (!frame)
                return;

        int current_row = row;
        const char *ptr = frame;

        if (centered)
                col = centered_indent;

        while (*ptr) {

                printf("\033[%d;%dH", current_row, col);
                clear_line();

                while (*ptr && *ptr != '\n') {
                        putchar(*ptr++);
                }

                if (*ptr == '\n')
                        ptr++;

                current_row++;
        }

        fflush(stdout);
        chroma_new_frame = 0;
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
