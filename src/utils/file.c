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
#include <errno.h>
#include <glib.h>
#include <libgen.h>
#include <pwd.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

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

        expand_path(path, expanded);

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

int expand_path(const char *input_path, char *expanded_path)
{
        if (input_path[0] == '\0' || input_path[0] == '\r')
                return -1;

        memset(expanded_path, 0, PATH_MAX);

        if (input_path[0] == '~') // Check if input_path starts with '~'
        {
                const char *home_dir;
                if (input_path[1] == '/' || input_path[1] == '\0') // Handle "~/"
                {
                        home_dir = getenv("HOME");
                        if (home_dir == NULL) {
                                return -1; // Unable to retrieve home directory
                        }
                        input_path++; // Skip '~' character
                } else                // Handle "~username/"
                {
                        const char *username = input_path + 1;
                        const char *slash = strchr(username, '/');
                        if (slash == NULL) {
                                const struct passwd *pw = getpwnam(username);
                                if (pw == NULL) {
                                        return -1; // Unable to retrieve user directory
                                }
                                home_dir = pw->pw_dir;
                                input_path = ""; // Empty path component after '~username'
                        } else {
                                size_t usernameLen = slash - username;
                                const struct passwd *pw = getpwuid(getuid());

                                if (pw == NULL) {
                                        return -1; // Unable to retrieve user directory
                                }
                                home_dir = pw->pw_dir;
                                input_path += usernameLen + 1; // Skip '~username/' component
                        }
                }

                size_t homeDirLen = strnlen(home_dir, PATH_MAX);
                size_t inputPathLen = strnlen(input_path, PATH_MAX);

                if (homeDirLen + inputPathLen >= PATH_MAX) {
                        return -1; // Expanded path exceeds maximum length
                }

                c_strcpy(expanded_path, home_dir, PATH_MAX);
                snprintf(expanded_path + homeDirLen, PATH_MAX - homeDirLen, "%s", input_path);
        } else // Handle if path is not prefixed with '~'
        {
                if (realpath(input_path, expanded_path) == NULL) {
                        return -1; // Unable to expand the path
                }
        }
        return 0; // Path expansion successful
}

void collapse_path(const char *input, char *output)
{
        if (!input || !output)
                return;

        size_t in_len = strlen(input);

        /* Quick copy for empty input */
        if (in_len == 0) {
                output[0] = '\0';
                return;
        }

        /* Resolve current user's home (prefer $HOME, fallback to getpwuid) */
        const char *home = getenv("HOME");
        if (!home) {
                struct passwd *pw = getpwuid(getuid());
                if (pw)
                        home = pw->pw_dir;
        }

        if (home) {
                size_t home_len = strlen(home);
                if (in_len >= home_len &&
                    strncmp(input, home, home_len) == 0 &&
                    (input[home_len] == '/' || input[home_len] == '\0')) {
                        /* Collapse to ~ or ~/rest */
                        if (input[home_len] == '\0') {
                                snprintf(output, PATH_MAX, "~");
                        } else {
                                snprintf(output, PATH_MAX, "~%s", input + home_len);
                        }
                        return;
                }
        }

#if !defined(__ANDROID__)
        /* Check other users' home dirs (e.g. /home/alice -> ~alice) */
        /* We'll iterate passwd entries and look for a pw_dir that is a prefix */
        struct passwd *pw;
        /* Iterate over passwd database */
        setpwent();
        while ((pw = getpwent()) != NULL) {
                if (!pw->pw_dir)
                        continue;
                size_t dlen = strlen(pw->pw_dir);
                if (in_len >= dlen &&
                    strncmp(input, pw->pw_dir, dlen) == 0 &&
                    (input[dlen] == '/' || input[dlen] == '\0')) {
                        /* Found a match for this user's home */
                        if (input[dlen] == '\0') {
                                /* exact match */
                                snprintf(output, PATH_MAX, "~%s", pw->pw_name);
                        } else {
                                snprintf(output, PATH_MAX, "~%s%s", pw->pw_name, input + dlen);
                        }
                        endpwent();
                        return;
                }
        }
        endpwent();
#endif

        /* No match — copy unchanged */
        snprintf(output, PATH_MAX, "%s", input);
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
        if (mkdir(path, 0700) == 0)
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

void generate_temp_file_path(char *file_path, const char *prefix, const char *suffix)
{
        const char *tmp_dir = getenv("TMPDIR");
        if (tmp_dir == NULL || strnlen(tmp_dir, PATH_MAX) >= PATH_MAX) {
                tmp_dir = "/tmp";
        }

        struct passwd *pw = getpwuid(getuid());
        const char *username = pw ? pw->pw_name : "unknown";

        char dir_path[PATH_MAX];
        snprintf(dir_path, sizeof(dir_path), "%s/kew", tmp_dir);
        create_directory(dir_path);
        snprintf(dir_path, sizeof(dir_path), "%s/kew/%s", tmp_dir, username);
        create_directory(dir_path);

        char random_string[7];
        for (int i = 0; i < 6; ++i) {
                random_string[i] = 'a' + rand() % 26;
        }
        random_string[6] = '\0';

        int written = snprintf(file_path, PATH_MAX, "%s/%s%.6s%s", dir_path, prefix, random_string, suffix);
        if (written < 0 || written >= PATH_MAX) {
                file_path[0] = '\0';
        }
}
