/**
 * @file choose_album_art.c
 * @brief Allows recursivively picking cover art in subdirectories
 *
 */

#include "choose_album_art.h"

#include "common/path_max.h"

#include "glib.h"
#include "glib/gstdio.h"
#include "utils/k_log.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <glib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>

static int compare_strings(const void *a, const void *b)
{
        return strcmp(*(char *const *)a, *(char *const *)b);
}

/* Search one directory (non-recursively) for the first entry whose
 * name matches any non-NULL element of names[0..size), preferring
 * lower indices over later matches. */
static char *search_dir_once(const char *dir_path, char **names, int size)
{
        GDir *directory = g_dir_open(dir_path, 0, NULL);
        if (!directory)
                return NULL;

        const char *entry_name;
        char *best_path = NULL;
        int best_index = size; /* lower is better; size == "no match yet" */

        while ((entry_name = g_dir_read_name(directory)) != NULL) {
                int matched_index = -1;
                for (int i = 0; i < size && i < best_index; i++) {
                        if (names[i] && strcmp(entry_name, names[i]) == 0) {
                                matched_index = i;
                                break;
                        }
                }
                if (matched_index < 0)
                        continue;

                char *candidate = g_build_filename(dir_path, entry_name, NULL);
                GStatBuf st;
                if (candidate && g_stat(candidate, &st) == 0 &&
                    S_ISREG(st.st_mode)) {
                        g_free(best_path);
                        best_path = candidate;
                        best_index = matched_index;
                } else {
                        g_free(candidate);
                }
        }

        g_dir_close(directory);
        return best_path;
}

/* Collect immediate subdirectory paths of dir_path, sorted so
 * traversal order is deterministic across filesystems/platforms. */
static char **collect_subdirs(const char *dir_path, int *out_count)
{
        *out_count = 0;

        GDir *directory = g_dir_open(dir_path, 0, NULL);
        if (!directory)
                return NULL;

        char **names = NULL;
        int count = 0;
        int capacity = 0;
        const char *entry_name;

        while ((entry_name = g_dir_read_name(directory)) != NULL) {
                char *candidate = g_build_filename(dir_path, entry_name, NULL);
                GStatBuf st;
                bool is_dir =
                    candidate && g_stat(candidate, &st) == 0 && S_ISDIR(st.st_mode);
                if (!is_dir) {
                        g_free(candidate);
                        continue;
                }

                if (count == capacity) {
                        capacity = capacity ? capacity * 2 : 8;
                        char **grown =
                            g_try_realloc(names, capacity * sizeof(*names));
                        if (!grown) {
                                g_free(candidate);
                                break; /* stop collecting; use what we have */
                        }
                        names = grown;
                }
                names[count++] = candidate;
        }

        g_dir_close(directory);

        if (count > 1)
                qsort(names, count, sizeof(*names), compare_strings);

        *out_count = count;
        return names;
}

static void free_subdirs(char **names, int count)
{
        for (int i = 0; i < count; i++)
                g_free(names[i]);
        g_free(names);
}

char *choose_album_art(const char *dir_path, char **custom_file_name_arr,
                        int arr_size, bool search_sub_dirs)
{
        if (!dir_path || !custom_file_name_arr || arr_size <= 0)
                return NULL;

        char *result = search_dir_once(dir_path, custom_file_name_arr, arr_size);
        if (result || !search_sub_dirs)
                return result;

        int subdir_count = 0;
        char **subdirs = collect_subdirs(dir_path, &subdir_count);

        for (int i = 0; i < subdir_count && !result; i++)
                result = search_dir_once(subdirs[i], custom_file_name_arr, arr_size);

        free_subdirs(subdirs, subdir_count);

        k_log("choose_album_art: returning: '%s'\n", result ? result : "(null)");

        return result;
}

