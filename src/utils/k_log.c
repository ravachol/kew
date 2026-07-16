#include "k_log.h"

#include "common/appstate.h"
#include "common/model.h"

#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>

char *k_log_get_error_log_path(void)
{
#ifdef _WIN32
        const char *base = getenv("LOCALAPPDATA");
        if (!base)
                return NULL;

        char dir[MAX_PATH];

        if (snprintf(dir, sizeof(dir), "%s\\kew", base) >= (int)sizeof(dir))
                return NULL;
        mkdir_p(dir);

        if (snprintf(dir, sizeof(dir), "%s\\kew\\logs", base) >= (int)sizeof(dir))
                return NULL;
        mkdir_p(dir);

        size_t len = snprintf(NULL, 0, "%s\\kew\\logs\\error.log", base);
        char *path = malloc(len + 1);
        if (!path)
                return NULL;

        snprintf(path, len + 1, "%s\\kew\\logs\\error.log", base);
        return path;

#elif defined(__APPLE__)
        const char *home = getenv("HOME");
        if (!home)
                return NULL;

        char dir[PATH_MAX];

        if (snprintf(dir, sizeof(dir), "%s/Library/Logs/kew", home) >= (int)sizeof(dir))
                return NULL;
        mkdir_p(dir);

        size_t len = snprintf(NULL, 0, "%s/Library/Logs/kew/error.log", home);
        char *path = malloc(len + 1);
        if (!path)
                return NULL;

        snprintf(path, len + 1, "%s/Library/Logs/kew/error.log", home);
        return path;

#else
        const char *state_dir = g_get_user_state_dir();

        size_t dir_len = snprintf(NULL, 0, "%s/kew/logs", state_dir);
        char *dir = malloc(dir_len + 1);
        if (!dir)
                return NULL;

        snprintf(dir, dir_len + 1, "%s/kew/logs", state_dir);

        if (g_mkdir_with_parents(dir, 0755) != 0) {
                free(dir);
                return NULL;
        }

        free(dir);

        size_t len = snprintf(NULL, 0, "%s/kew/logs/error.log", state_dir);
        char *path = malloc(len + 1);
        if (!path)
                return NULL;

        snprintf(path, len + 1, "%s/kew/logs/error.log", state_dir);

        return path;
#endif
}

void k_log_init(void)
{
        char *path = k_log_get_error_log_path();

        if (path != NULL) {
                Model *model = get_model();

                char *dir = strdup(path); // Duplicate because dirname can change the path

                if (dir != NULL) {
                        g_mkdir_with_parents(dirname(dir), 0755);
                        free(dir);
                }

                model->state.ui.logFile = freopen(path, "w", stderr);

                if (model->state.ui.logFile)
                        setvbuf(stderr, NULL, _IONBF, 0);

                free(path);
        }
}

void k_log_shutdown(void)
{
        Model *model = get_model();

        if (model->state.ui.logFile)
                fclose(model->state.ui.logFile);
}

void k_log(const char *fmt, ...)
{
        if (!get_model()->state.ui.logFile)
                return;

        va_list ap;
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
        fputc('\n', stderr);
        fflush(stderr);
}
