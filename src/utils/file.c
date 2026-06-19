/**
 * @file file.c
 * @brief File and directory utilities.
 *
 * Provides wrappers around file I/O, path manipulation,
 * and safe filesystem access used throughout the app.
 */

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "file.h"

#include "utils.h"

#include <dirent.h>
#include <glib.h>
#include <libgen.h>
#include <regex.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir_p(path) _mkdir(path)
#else
#include <sys/types.h>
#include <errno.h>
#include <pwd.h>
#endif


#define MAX_RECURSION_DEPTH 64

const char *get_temp_dir()
{
        const char *tmpdir = getenv("TMPDIR");
        if (tmpdir != NULL) {
                return tmpdir; // Use TMPDIR if set (common on Android/Termux)
        }

        tmpdir = getenv("TEMP");
        if (tmpdir != NULL) {
                return tmpdir;
        }

        // Fallback to /tmp on Unix-like systems
        return "/tmp";
}

void get_directory_from_path(const char *path, char *directory)
{
        if (!path || !directory)
                return;

        size_t len = strnlen(path, PATH_MAX);

        char *tmp = malloc(len + 1);
        if (!tmp) {
                fprintf(stderr, "Out of memory while processing path\n");
                return;
        }

        memcpy(tmp, path, len + 1);

        // dirname() may modify the buffer, so we keep it in tmp
        char *dir = dirname(tmp);

        // Copy the result to the caller‑supplied buffer safely
        strncpy(directory, dir, PATH_MAX - 1);
        directory[PATH_MAX - 1] = '\0'; // Ensure null termination

        /// Ensure a trailing '/'
        size_t dlen = strnlen(directory, PATH_MAX);

        if (dlen > 0 && directory[dlen - 1] != '/' &&
            dlen + 1 < PATH_MAX) {
                directory[dlen] = '/';
                directory[dlen + 1] = '\0';
        }

        free(tmp);
}

int exists_file(const char *fname)
{
        if (fname == NULL || fname[0] == '\0')
                return -1;

        FILE *file;
        if ((file = fopen(fname, "r"))) {
                fclose(file);
                return 1;
        }
        return -1;
}

int is_directory(const char *path)
{
        DIR *dir = opendir(path);
        if (dir) {
                closedir(dir);
                return 1;
        } else {
                if (errno == ENOENT) {
                        return -1;
                }
                return 0;
        }
}

int directory_exists(const char *path)
{
        char expanded[PATH_MAX];

        expand_path(path, expanded, PATH_MAX);

        DIR *dir = opendir(expanded);

        if (dir) {
                closedir(dir);
                return 1;
        }
        return 0;
}

// Traverse a directory tree and search for a given file or directory
int walker(const char *start_path, const char *low_case_searching, char *result,
           const char *allowed_extensions, enum SearchType search_type, bool exact_search, int depth)
{
        if (depth > MAX_RECURSION_DEPTH) {
                fprintf(stderr, "Maximum recursion depth exceeded\n");
                return 1;
        }

        if (!start_path || !low_case_searching || !result || !allowed_extensions) {
                fprintf(stderr, "Invalid arguments to walker\n");
                return 1;
        }

        struct stat path_stat;
        if (stat(start_path, &path_stat) != 0) {
                fprintf(stderr, "Cannot stat path '%s': %s\n", start_path, strerror(errno));
                return 1;
        }

        if (!S_ISDIR(path_stat.st_mode)) {
                // Not a directory, stop here
                return 1;
        }

        DIR *d = opendir(start_path);
        if (!d) {
                fprintf(stderr, "Failed to open directory '%s': %s\n", start_path, strerror(errno));
                return 1;
        }

        regex_t regex;
        if (regcomp(&regex, allowed_extensions, REG_EXTENDED) != 0) {
                fprintf(stderr, "Failed to compile regex\n");
                closedir(d);
                return 1;
        }

        bool found = false;
        struct dirent *entry;
        char ext[100] = {0};

        while ((entry = readdir(d)) != NULL) {
                // Skip . and ..
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                        continue;

                // Build full path for entry
                char entry_path[PATH_MAX];
                if (snprintf(entry_path, sizeof(entry_path), "%s/%s", start_path, entry->d_name) >= (int)sizeof(entry_path)) {
                        fprintf(stderr, "Path too long: %s/%s\n", start_path, entry->d_name);
                        continue;
                }

                if (stat(entry_path, &path_stat) != 0) {
                        // Can't stat, skip
                        continue;
                }

                if (S_ISDIR(path_stat.st_mode)) {
                        // Directory handling
                        char *folded_name = g_utf8_casefold(entry->d_name, -1);
                        if (!folded_name)
                                continue;

                        bool nameMatch = exact_search
                                             ? (strcasecmp(folded_name, low_case_searching) == 0)
                                             : (c_strcasestr(folded_name, low_case_searching, PATH_MAX) != NULL);

                        free(folded_name);

                        if (nameMatch && search_type != FileOnly && search_type != SearchPlayList) {
                                strncpy(result, entry_path, PATH_MAX - 1);
                                result[PATH_MAX - 1] = '\0';
                                found = true;
                                break;
                        }

                        // Recurse into subdirectory
                        if (walker(entry_path, low_case_searching, result, allowed_extensions, search_type, exact_search, depth + 1) == 0) {
                                found = true;
                                break;
                        }
                } else {
                        // File handling
                        if (search_type == DirOnly)
                                continue;

                        if (strlen(entry->d_name) <= 4)
                                continue;

                        extract_extension(entry->d_name, sizeof(ext) - 1, ext);
                        if (match_regex(&regex, ext) != 0)
                                continue;

                        char *folded_name = g_utf8_casefold(entry->d_name, -1);
                        if (!folded_name)
                                continue;

                        bool nameMatch = exact_search
                                             ? (strcasecmp(folded_name, low_case_searching) == 0)
                                             : (c_strcasestr(folded_name, low_case_searching, PATH_MAX) != NULL);

                        free(folded_name);

                        if (nameMatch) {
                                strncpy(result, entry_path, PATH_MAX - 1);
                                result[PATH_MAX - 1] = '\0';
                                found = true;
                                break;
                        }
                }
        }

        regfree(&regex);
        closedir(d);

        return found ? 0 : 1;
}

int expand_path(const char *input, char *out, size_t max_size)
{
    if (!input || !out || max_size == 0)
        return -1;

    if (input[0] == '\0' || input[0] == '\r')
        return -1;

    const char *home = get_home_path();
    if (!home)
        return -1;

    /* Handle ~ */
    if (input[0] == '~' &&
        (input[1] == '\0' || input[1] == '/')) {

        size_t home_len = strlen(home);
        size_t rest_len = strlen(input + 1); /* includes "/" or "\0" */

        if (home_len + rest_len + 1 > max_size)
            return -1;

        memcpy(out, home, home_len);
        memcpy(out + home_len, input + 1, rest_len + 1); /* include null */
        return 0;
    }

#ifdef _WIN32
    /* Windows/MSYS2: do NOT rely on realpath always */
    if (_fullpath(out, input, max_size) == NULL)
        return -1;
#else
    if (!realpath(input, out))
        return -1;
#endif

    return 0;
}

void collapse_path(const char *input, char *out, size_t max_size)
{
    if (!input || !out || max_size == 0) {
        if (out && max_size) out[0] = '\0';
        return;
    }

    const char *home = get_home_path();
    if (!home) {
        snprintf(out, max_size, "%s", input);
        return;
    }

    size_t home_len = strlen(home);
    size_t in_len = strlen(input);

    /* match HOME prefix */
    if (in_len >= home_len &&
        strncmp(input, home, home_len) == 0 &&
        (input[home_len] == '/' || input[home_len] == '\0')) {

        if (input[home_len] == '\0') {
            snprintf(out, max_size, "~");
        } else {
            snprintf(out, max_size, "~%s", input + home_len);
        }
        return;
    }

    snprintf(out, max_size, "%s", input);
}

static int dir_mkdir(const char *path)
{
#ifdef _WIN32
    return mkdir(path);
#else
    return mkdir(path, 0700);
#endif
}

int create_directory(const char *path)
{
        struct stat st;

        // Check if directory already exists
        if (stat(path, &st) == 0) {
                if (S_ISDIR(st.st_mode))
                        return 0; // Directory already exists
                else
                        return -1; // Path exists but is not a directory
        }

        // Directory does not exist, so create it
        if (dir_mkdir(path) == 0)
                return 1; // Directory created successfully

        return -1; // Failed to create directory
}

int delete_file(const char *file_path)
{
        if (remove(file_path) == 0) {
                return 0;
        } else {
                return -1;
        }
}

int is_in_temp_dir(const char *path)
{
        const char *tmp_dir = getenv("TMPDIR");
        static char tmpdir_buf[PATH_MAX + 2];

        if (tmp_dir == NULL || strnlen(tmp_dir, PATH_MAX) >= PATH_MAX)
                tmp_dir = "/tmp";

        size_t len = strlen(tmp_dir);
        strncpy(tmpdir_buf, tmp_dir, PATH_MAX);
        tmpdir_buf[PATH_MAX] = '\0';

        if (len == 0 || tmpdir_buf[len - 1] != '/') {
                tmpdir_buf[len] = '/';
                tmpdir_buf[len + 1] = '\0';
        }

        return path_starts_with(path, tmpdir_buf);
}

static const char *get_tmp_dir(void)
{
#ifdef _WIN32
    const char *tmp = getenv("TEMP");
    if (tmp && *tmp) return tmp;

    tmp = getenv("TMP");
    if (tmp && *tmp) return tmp;

    static char win_tmp[MAX_PATH];
    DWORD len = GetTempPathA(MAX_PATH, win_tmp);
    if (len > 0 && len < MAX_PATH)
        return win_tmp;

    return "C:\\Temp";
#else
    const char *tmp = getenv("TMPDIR");
    return (tmp && *tmp) ? tmp : "/tmp";
#endif
}

void generate_temp_file_path(char *file_path,
                             size_t max_size,
                             const char *prefix,
                             const char *suffix)
{
    if (!file_path || max_size == 0) return;

    const char *tmp_dir = get_tmp_dir();

    /* username (optional, Windows-safe fallback) */
    const char *username = getenv("USER");
#ifdef _WIN32
    if (!username) username = getenv("USERNAME");
#endif
    if (!username) username = "unknown";

    char base_dir[512];
    char user_dir[512];

#ifdef _WIN32
    snprintf(base_dir, sizeof(base_dir), "%s\\kew", tmp_dir);
    snprintf(user_dir, sizeof(user_dir), "%s\\kew\\%s", tmp_dir, username);
#else
    snprintf(base_dir, sizeof(base_dir), "%s/kew", tmp_dir);
    snprintf(user_dir, sizeof(user_dir), "%s/kew/%s", tmp_dir, username);
#endif

    create_directory(base_dir);
    create_directory(user_dir);

    /* random string */
    char rnd[7];
    for (int i = 0; i < 6; i++)
        rnd[i] = 'a' + (rand() % 26);
    rnd[6] = '\0';

    /* final path */
#ifdef _WIN32
    int written = snprintf(file_path, max_size, "%s\\%s%s%s",
                           user_dir, prefix, rnd, suffix);
#else
    int written = snprintf(file_path, max_size, "%s/%s%s%s",
                           user_dir, prefix, rnd, suffix);
#endif

    if (written < 0 || (size_t)written >= max_size) {
        file_path[0] = '\0';
    }
}

char *path_realpath(const char *path, char *out)
{
#if defined(_WIN32)
        return _fullpath(out, path, _MAX_PATH);
#else
        return realpath(path, out);
#endif
}

