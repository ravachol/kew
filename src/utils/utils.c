/**
 * @file utils.c
 * @brief General-purpose utility functions and helpers.
 *
 * Provides miscellaneous helpers used across modules such as
 * string operations, math functions, or logging utilities.
 */

#include "utils.h"

#include <ctype.h>
#include <fcntl.h>
#include <glib.h>
#include <math.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#ifdef _WIN32
#include <io.h>
#include <shlobj.h>
#include <windows.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

struct timespec timer_start;

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
    defined(__NetBSD__)
#include <stdint.h> // For uint32_t
#include <stdlib.h> // For arc4random

uint32_t arc4random_uniform(uint32_t upper_bound);

int get_random_number(int min, int max)
{
        return min + arc4random_uniform(max - min + 1);
}

#else
#include <stdlib.h>
#include <time.h>

int get_random_number(int min, int max)
{
        return min +
               (rand() % (max - min + 1));
}

#endif

void c_sleep(int milliseconds)
{
        struct timespec ts;
        ts.tv_sec = milliseconds / 1000;
        ts.tv_nsec = (milliseconds % 1000) * 1000000;

        if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec += ts.tv_nsec / 1000000000;
                ts.tv_nsec %= 1000000000;
        }

        nanosleep(&ts, NULL);
}

void c_usleep(int microseconds)
{
        if (microseconds < 0 || microseconds > 100000000) {
                return;
        }

        struct timespec ts;
        ts.tv_sec = microseconds / 1000000;
        ts.tv_nsec = (microseconds % 1000000) * 1000;

        nanosleep(&ts, NULL);
}

void c_strcpy(char *dest, const char *src, size_t dest_size)
{
        if (!dest || dest_size == 0)
                return;

        if (!src) {
                dest[0] = '\0';
                return;
        }

        size_t len = strnlen(src, dest_size - 1);

        memcpy(dest, src, len);
        dest[len] = '\0';
}

gint64 get_length_in_micro_sec(double duration)
{
        return floor(llround(duration * G_USEC_PER_SEC));
}

char *string_to_lower(const char *str)
{
        if (str == NULL) {
                return NULL;
        }

        size_t length = strnlen(str, KEW_PATH_MAX);

        return g_utf8_strdown(str, length);
}

char *string_to_upper(const char *str)
{
        if (str == NULL) {
                return NULL;
        }

        size_t length = strnlen(str, KEW_PATH_MAX);

        return g_utf8_strup(str, length);
}

char *c_strcasestr(const char *haystack, const char *needle, int max_scan_len)
{
        if (!haystack || !needle || max_scan_len <= 0)
                return NULL;

        size_t needle_len = strnlen(needle, max_scan_len);

        if (needle_len == 0)
                return (char *)haystack;

        size_t haystack_len = strnlen(haystack, max_scan_len);

        if (needle_len > haystack_len)
                return NULL;

        for (size_t i = 0; i <= haystack_len - needle_len; i++) {
                if (strncasecmp(&haystack[i], needle, needle_len) == 0) {
                        return (char *)(haystack + i);
                }
        }

        return NULL;
}

int match_regex(const regex_t *regex, const char *ext)
{
        if (regex == NULL || ext == NULL) {
                fprintf(stderr, "Invalid arguments\n");
                return 1;
        }

        regmatch_t pmatch[1];
        int ret = regexec(regex, ext, 1, pmatch, 0);

        if (ret == REG_NOMATCH) {
                return 1;
        } else if (ret == 0) {
                return 0;
        } else {
                fprintf(stderr, "match_regex: Regex match failed");

                return 1;
        }
}

bool is_valid_utf8(const char *str, size_t len)
{
        size_t i = 0;
        while (i < len) {
                unsigned char c = str[i];
                if (c <= 0x7F) // 1-byte ASCII character
                {
                        i++;
                } else if ((c & 0xE0) == 0xC0) // 2-byte sequence
                {
                        if (i + 1 >= len || (str[i + 1] & 0xC0) != 0x80)
                                return false;
                        i += 2;
                } else if ((c & 0xF0) == 0xE0) // 3-byte sequence
                {
                        if (i + 2 >= len || (str[i + 1] & 0xC0) != 0x80 ||
                            (str[i + 2] & 0xC0) != 0x80)
                                return false;
                        i += 3;
                } else if ((c & 0xF8) == 0xF0) // 4-byte sequence
                {
                        if (i + 3 >= len || (str[i + 1] & 0xC0) != 0x80 ||
                            (str[i + 2] & 0xC0) != 0x80 ||
                            (str[i + 3] & 0xC0) != 0x80)
                                return false;
                        i += 4;
                } else {
                        return false; // Invalid UTF-8
                }
        }
        return true;
}

void extract_extension(const char *filename, size_t ext_size, char *ext)
{
        if (!filename || !ext || ext_size == 0) {
                if (ext && ext_size > 0)
                        ext[0] = '\0';
                return;
        }

        size_t length = strnlen(filename, KEW_PATH_MAX);

        // Find the last '.' character in the filename
        const char *dot = NULL;
        for (size_t i = 0; i < length; i++) {
                if (filename[i] == '.') {
                        dot = &filename[i];
                }
        }

        // If no dot was found, there's no extension
        if (!dot || dot == filename + length - 1) {
                ext[0] = '\0'; // No extension found
                return;
        }

        size_t i = 0, j = 0;
        size_t dot_pos = dot - filename + 1;

        // Copy the extension while checking for UTF-8 validity
        while (dot_pos + i < length && filename[dot_pos + i] != '\0' &&
               j < ext_size - 1) {
                size_t char_size = 1; // Default to 1 byte (ASCII)

                unsigned char c = filename[dot_pos + i];
                if ((c & 0x80) != 0) // Check if the character is multi-byte
                {
                        if ((c & 0xE0) == 0xC0) // 2-byte sequence
                                char_size = 2;
                        else if ((c & 0xF0) == 0xE0) // 3-byte sequence
                                char_size = 3;
                        else if ((c & 0xF8) == 0xF0) // 4-byte sequence
                                char_size = 4;
                        else {
                                break; // Invalid UTF-8 start byte
                        }
                }

                // Ensure we don't overflow the destination buffer
                if (j + char_size >= ext_size)
                        break;

                // Check if the character is valid UTF-8
                if (is_valid_utf8(&filename[dot_pos + i], char_size)) {
                        // Copy the character to the extension buffer
                        memcpy(ext + j, filename + dot_pos + i, char_size);
                        j += char_size;
                        i += char_size;
                } else {
                        break; // Invalid UTF-8, stop copying
                }
        }

        // Null-terminate the extension
        ext[j] = '\0';
}

int path_ends_with(const char *str, const char *suffix)
{
        size_t length = strnlen(str, KEW_PATH_MAX);
        size_t suffixLength = strnlen(suffix, KEW_PATH_MAX);

        if (suffixLength > length) {
                return 0;
        }

        const char *str_suffix = str + (length - suffixLength);
        return strcasecmp(str_suffix, suffix) == 0;
}

int path_starts_with(const char *str, const char *prefix)
{
        size_t length = strnlen(str, KEW_PATH_MAX);
        size_t prefixLength = strnlen(prefix, KEW_PATH_MAX);

        if (prefixLength > length) {
                return 0;
        }

        return strncmp(str, prefix, prefixLength) == 0;
}

void trim(char *str, size_t max_len)
{
        if (!str || max_len == 0) {
                return;
        }

        char *start = str;
        char *limit = str + max_len;

        // Skip leading whitespace
        while (start < limit && *start && isspace((unsigned char)*start)) {
                start++;
        }

        if (start == limit || *start == '\0') {
                str[0] = '\0';
                return;
        }

        // Find end
        char *end = start;
        while (end < limit && *end) {
                end++;
        }
        end--; // last character

        while (end >= start && isspace((unsigned char)*end)) {
                end--;
        }

        *(end + 1) = '\0';

        if (start != str) {
                memmove(str, start, (end - start) + 2);
        }
}

const char *get_home_path(void)
{
#ifdef _WIN32
        const char *home = getenv("USERPROFILE");
        if (home)
                return home;

        static char path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, path))) {
                return path;
        }
        return NULL;
#else
        const char *home = getenv("HOME");
        if (home)
                return home;

        struct passwd *pw = getpwuid(getuid());
        return pw ? pw->pw_dir : NULL;
#endif
}

static const char *get_xdg_config_base(void)
{
#ifdef _WIN32
        const char *appdata = getenv("APPDATA");
        return appdata;
#else
        return getenv("XDG_CONFIG_HOME");
#endif
}

static const char *get_xdg_state_base(void)
{
#ifdef _WIN32
        const char *local = getenv("LOCALAPPDATA");
        return local;
#else
        return getenv("XDG_STATE_HOME");
#endif
}

char *get_config_path(void)
{
        char *out = malloc(KEW_PATH_MAX);
        if (!out)
                return NULL;

        const char *base = get_xdg_config_base();
        const char *home = get_home_path();

        if (base) {
#ifdef _WIN32
                snprintf(out, KEW_PATH_MAX, "%s\\kew", base);
#else
                snprintf(out, KEW_PATH_MAX, "%s/kew", base);
#endif
        } else if (home) {
#ifdef __APPLE__
                snprintf(out, KEW_PATH_MAX, "%s/Library/Preferences/kew", home);
#elif defined(_WIN32)
                snprintf(out, KEW_PATH_MAX, "%s\\kew", home);
#else
                snprintf(out, KEW_PATH_MAX, "%s/.config/kew", home);
#endif
        } else {
                free(out);
                return NULL;
        }

        return out;
}

char *get_prefs_path(void)
{
        char *out = malloc(KEW_PATH_MAX);
        if (!out)
                return NULL;

        const char *base = get_xdg_state_base();
        const char *home = get_home_path();

        if (base) {
                snprintf(out, KEW_PATH_MAX, "%s", base);
        } else if (home) {
#ifdef __APPLE__
                snprintf(out, KEW_PATH_MAX, "%s/Library/Application Support", home);
#elif _WIN32
                snprintf(out, KEW_PATH_MAX, "%s\\AppData\\Local", home);
#else
                snprintf(out, KEW_PATH_MAX, "%s/.local/state", home);
#endif
        } else {
                free(out);
                return NULL;
        }

        return out;
}
bool is_valid_filename(const char *filename)
{
        // Check for path traversal patterns
        if (strstr(filename, "..") != NULL) {
                return false;
        }

        // Check for path separators (works for UTF-8)
        if (strchr(filename, '/') != NULL || strchr(filename, '\\') != NULL) {
                return false;
        }

        // Don't allow starting with dot (hidden files)
        if (filename[0] == '.') {
                return false;
        }

        // Allow everything else (including UTF-8 Chinese characters)
        return true;
}

char *get_file_path(const char *filename)
{
        if (filename == NULL || !is_valid_filename(filename)) {
                return NULL;
        }

        if (filename[0] == '.') {
                return NULL;
        }

        char *configdir = get_config_path();
        if (configdir == NULL) {
                return NULL;
        }

        size_t configdir_length = strnlen(configdir, KEW_PATH_MAX);
        size_t filename_length = strnlen(filename, KEW_PATH_MAX);

        size_t filepath_length = configdir_length + 1 + filename_length + 1;

        if (filepath_length > KEW_PATH_MAX) {
                free(configdir);
                return NULL;
        }

        char *filepath = (char *)malloc(filepath_length);
        if (filepath == NULL) {
                free(configdir);
                return NULL;
        }

        snprintf(filepath, filepath_length, "%s/%s", configdir, filename);

        free(configdir);
        return filepath;
}

void format_filename(char *str)
{
        // Replace underscores with spaces
        for (int j = 0; str[j] != '\0'; j++) {
                if (str[j] == '_') {
                        str[j] = ' ';
                }
        }

        // Trim a decimal number followed by a dash, period, or comma (ignoring whitespace)
        regex_t regex;
        if (regcomp(&regex, STRIP_TRACK_NUMBER, REG_EXTENDED | REG_ICASE) != 0) {
                return;
        }

        regmatch_t match[1];
        if (regexec(&regex, str, 1, match, 0) != 0) {
                regfree(&regex);
                return;
        }

        if (strlen(str + match->rm_eo) == 0) {
                regfree(&regex);
                return; // if removing leaves empty string
        }

        memmove(str, str + match->rm_eo, strlen(str + match->rm_eo) + 1);
        regfree(&regex);
}

void shorten_string(char *str, size_t max_length)
{
        size_t length = strnlen(str, max_length + 2);

        if (length > max_length) {
                str[max_length] = '\0';
        }
}

void print_blank_spaces(int num_spaces)
{
        if (num_spaces < 1)
                return;
        printf("%*s", num_spaces, " ");
}

int get_number(const char *str)
{
        char *endptr;
        long value = strtol(str, &endptr, 10);

        if (value < INT_MIN || value > INT_MAX) {
                return 0;
        }

        return (int)value;
}

float get_float(const char *str)
{
        char *endptr;
        float value = strtof(str, &endptr);

        if (str == endptr) {
                return 0.0f;
        }

        if (isnan(value) || isinf(value) || value < -FLT_MAX || value > FLT_MAX) {
                return 0.0f;
        }

        return value;
}

static int file_sync(int fd)
{
#ifdef _WIN32
        /* Windows equivalent */
        return _commit(fd);
#else
        return fsync(fd);
#endif
}

int copy_file(const char *src, const char *dst)
{
        // Validate inputs
        if (!src || !dst) {
                fprintf(stderr, "copy_file: one or both of the inputs are null\n");
                return -1;
        }

        // Check if source and destination are the same
        struct stat src_stat, dst_stat;
        if (stat(src, &src_stat) != 0) {
                fprintf(stderr, "copy_file: source and destination are the same\n");
                return -1;
        }

        // Don't copy if destination exists and is the same file (same inode)
        if (stat(dst, &dst_stat) == 0) {
                if (src_stat.st_dev == dst_stat.st_dev &&
                    src_stat.st_ino == dst_stat.st_ino) {
                        fprintf(stderr, "copy_file: the same file exists at the destination\n");
                        return -1; // Same file
                }
        }

        // Don't copy directories, symlinks, or special files
        if (!S_ISREG(src_stat.st_mode)) {
                fprintf(stderr, "copy_file: file is of wrong type\n");
                return -1;
        }

        // Check file size is reasonable (prevent copying huge files)
        if (src_stat.st_size > 10 * 1024 * 1024) { // 10MB limit for theme files
                fprintf(stderr, "copy_file: file too large\n");
                return -1;
        }

        // Open source file
        int src_fd = open(src, O_RDONLY);
        if (src_fd < 0) {
                perror("copy_file");
                return -1;
        }

        // Create destination with user read/write permissions
        int dst_fd = open(dst, O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (dst_fd < 0) {
                // If file exists, try to open it (but don't use O_EXCL)
                dst_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0600);
                if (dst_fd < 0) {
                        close(src_fd);
                        perror("copy_file");
                        return -1;
                }
        }

        // Copy data in chunks
        char buffer[8192];
        ssize_t bytes_read;
        ssize_t total_written = 0;

        while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0) {
                ssize_t total = 0;

                while (total < bytes_read) {
                        ssize_t bytes_written = write(dst_fd,
                                                      buffer + total,
                                                      bytes_read - total);

                        if (bytes_written < 0) {
                                if (errno == EINTR)
                                        continue;
                                close(src_fd);
                                close(dst_fd);
                                unlink(dst);
                                perror("copy_file");
                                return -1;
                        }

                        total_written += bytes_written;

                        // Sanity check: make sure we're not writing more than expected
                        if (total_written > src_stat.st_size) {
                                close(src_fd);
                                close(dst_fd);
                                unlink(dst);
                                fprintf(stderr, "copy_file: file too large\n");
                                return -1;
                        }

                        total += bytes_written;
                }
        }

        if (bytes_read < 0) {
                close(src_fd);
                close(dst_fd);
                unlink(dst); // Remove partial file on error
                return -1;
        }

        // Sync to disk before closing
        if (file_sync(dst_fd) != 0) {
                close(src_fd);
                close(dst_fd);
                unlink(dst);
                return -1;
        }

        close(src_fd);
        close(dst_fd);

        return 0;
}

int get_number_from_string(const char *str)
{
        char *endptr;
        long value = strtol(str, &endptr, 10);

        if (*endptr != '\0') {
                return 0;
        }

        if (value < 0 || value > INT_MAX) {
                return 0;
        }

        return (int)value;
}

void start_timer(void)
{
        clock_gettime(CLOCK_MONOTONIC, &timer_start);
        fprintf(stderr, "Timer started\n");
}

void end_timer(void)
{
        struct timespec timer_end;
        clock_gettime(CLOCK_MONOTONIC, &timer_end);

        double elapsed =
            (timer_end.tv_sec - timer_start.tv_sec) +
            (timer_end.tv_nsec - timer_start.tv_nsec) / 1e9;

        fprintf(stderr, "Timer ended: %.6f seconds elapsed\n", elapsed);
}
