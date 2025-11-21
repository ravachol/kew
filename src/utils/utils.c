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
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

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
        static int seeded = 0;
        if (!seeded) {
                srand(time(
                    NULL));
                seeded = 1;
        }

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
        if (microseconds < 0 || microseconds > 100000000)
        {
                return;
        }

        struct timespec ts;
        ts.tv_sec = microseconds / 1000000;
        ts.tv_nsec = (microseconds % 1000000) * 1000;

        nanosleep(&ts, NULL);
}

void c_strcpy(char *dest, const char *src, size_t dest_size)
{
        if (dest && src && dest_size > 0) {
                size_t src_length = strnlen(src, dest_size - 1);
                if (src_length >= dest_size)
                        src_length = dest_size - 1;

                memcpy(dest, src, src_length);

                dest[src_length] = '\0';
        } else if (dest && dest_size > 0)
        {
                dest[0] = '\0';
        }
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

        size_t length = strnlen(str, PATH_MAX);

        return g_utf8_strdown(str, length);
}

char *string_to_upper(const char *str)
{
        if (str == NULL) {
                return NULL;
        }

        size_t length = strnlen(str, PATH_MAX);

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

        size_t length = strnlen(filename, PATH_MAX);

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
        size_t length = strnlen(str, PATH_MAX);
        size_t suffixLength = strnlen(suffix, PATH_MAX);

        if (suffixLength > length) {
                return 0;
        }

        const char *str_suffix = str + (length - suffixLength);
        return strcmp(str_suffix, suffix) == 0;
}

int path_starts_with(const char *str, const char *prefix)
{
        size_t length = strnlen(str, PATH_MAX);
        size_t prefixLength = strnlen(prefix, PATH_MAX);

        if (prefixLength > length) {
                return 0;
        }

        return strncmp(str, prefix, prefixLength) == 0;
}

void trim(char *str, int max_len)
{
        if (!str || max_len <= 0) {
                return;
        }

        // Find start (skip leading whitespace)
        char *start = str;
        while (*start && isspace(*start)) {
                start++;
        }

        // Handle case where string is all whitespace or empty
        size_t len = strnlen(start, max_len - (start - str));
        if (len == 0) {
                str[0] = '\0';
                return;
        }

        // Find end (skip trailing whitespace)
        char *end = start + len - 1;
        while (end >= start && isspace(*end)) {
                end--;
        }

        // Null terminate
        *(end + 1) = '\0';

        // Move trimmed string to beginning if needed
        if (start != str) {
                size_t trimmed_len = end - start + 1;
                memmove(str, start, trimmed_len + 1); // +1 for null terminator
        }
}

const char *get_home_path(void)
{
        struct passwd *pw = getpwuid(getuid());
        if (pw && pw->pw_dir) {
                return pw->pw_dir;
        }
        return NULL;
}

char *get_config_path(void)
{
        char *config_path = malloc(PATH_MAX);
        if (!config_path)
                return NULL;

        const char *xdg_config = getenv("XDG_CONFIG_HOME");

        if (xdg_config) {
                snprintf(config_path, PATH_MAX, "%s/kew", xdg_config);
        } else {
                const char *home = get_home_path();
                if (home) {
#ifdef __APPLE__
                        snprintf(config_path, PATH_MAX,
                                 "%s/Library/Preferences/kew", home);
#else
                        snprintf(config_path, PATH_MAX, "%s/.config/kew",
                                 home);
#endif
                } else {
                        struct passwd *pw = getpwuid(getuid());
                        if (pw) {
#ifdef __APPLE__
                                snprintf(config_path, PATH_MAX,
                                         "%s/Library/Preferences/kew",
                                         pw->pw_dir);
#else
                                snprintf(config_path, PATH_MAX,
                                         "%s/.config/kew", pw->pw_dir);
#endif
                        } else {
                                free(config_path);
                                return NULL;
                        }
                }
        }

        return config_path;
}

char *get_prefs_path(void)
{
        char *prefs_path = malloc(PATH_MAX);
        if (!prefs_path)
                return NULL;

        const char *xdg_state = getenv("XDG_STATE_HOME");

        if (xdg_state) {
                snprintf(prefs_path, PATH_MAX, "%s", xdg_state);
        } else {
                const char *home = get_home_path();
                if (home) {
#ifdef __APPLE__
                        snprintf(prefs_path, PATH_MAX, "%s/Library/Application Support", home);
#else
                        snprintf(prefs_path, PATH_MAX, "%s/.local/state", home);
#endif
                } else {
                        struct passwd *pw = getpwuid(getuid());
                        if (pw) {
#ifdef __APPLE__
                                snprintf(prefs_path, PATH_MAX, "%s/Library/Application Support", pw->pw_dir);
#else
                                snprintf(prefs_path, PATH_MAX, "%s/.local/state", pw->pw_dir);
#endif
                        } else {
                                free(prefs_path);
                                return NULL;
                        }
                }
        }

        return prefs_path;
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

        size_t configdir_length = strnlen(configdir, PATH_MAX);
        size_t filename_length = strnlen(filename, PATH_MAX);

        size_t filepath_length = configdir_length + 1 + filename_length + 1;

        if (filepath_length > PATH_MAX) {
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

void remove_unneeded_chars(char *str, int length)
{
        // Do not remove characters if filename only contains digits
        bool stringContainsLetters = false;
        for (int i = 0; str[i] != '\0'; i++) {
                if (!isdigit(str[i])) {
                        stringContainsLetters = true;
                }
        }
        if (!stringContainsLetters) {
                return;
        }

        for (int i = 0; i < 3 && str[i] != '\0' && str[i] != ' '; i++) {
                if (isdigit(str[i]) || str[i] == '.' || str[i] == '-' ||
                    str[i] == ' ') {
                        int j;
                        for (j = i; str[j] != '\0'; j++) {
                                str[j] = str[j + 1];
                        }
                        str[j] = '\0';
                        i--; // Decrement i to re-check the current index
                        length--;
                }
        }

        // Remove hyphens and underscores from filename
        for (int i = 0; str[i] != '\0'; i++) {
                // Only remove if there are no spaces around
                if ((str[i] == '-' || str[i] == '_') &&
                    (i > 0 && i < length && str[i - 1] != ' ' &&
                     str[i + 1] != ' ')) {
                        str[i] = ' ';
                }
        }
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

int copy_file(const char *src, const char *dst)
{
        // Validate inputs
        if (!src || !dst) {
                return -1;
        }

        // Check if source and destination are the same
        struct stat src_stat, dst_stat;
        if (stat(src, &src_stat) != 0) {
                return -1;
        }

        // Don't copy if destination exists and is the same file (same inode)
        if (stat(dst, &dst_stat) == 0) {
                if (src_stat.st_dev == dst_stat.st_dev &&
                    src_stat.st_ino == dst_stat.st_ino) {
                        return -1; // Same file
                }
        }

        // Don't copy directories, symlinks, or special files
        if (!S_ISREG(src_stat.st_mode)) {
                return -1;
        }

        // Check file size is reasonable (prevent copying huge files)
        if (src_stat.st_size > 10 * 1024 * 1024) { // 10MB limit for theme files
                return -1;
        }

        // Open source file
        int src_fd = open(src, O_RDONLY);
        if (src_fd < 0) {
                return -1;
        }

        // Create destination with user read/write permissions
        int dst_fd = open(dst, O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (dst_fd < 0) {
                // If file exists, try to open it (but don't use O_EXCL)
                dst_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0600);
                if (dst_fd < 0) {
                        close(src_fd);
                        return -1;
                }
        }

        // Copy data in chunks
        char buffer[8192];
        ssize_t bytes_read, bytes_written;
        ssize_t total_written = 0;

        while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0) {
                bytes_written = write(dst_fd, buffer, bytes_read);
                if (bytes_written != bytes_read) {
                        close(src_fd);
                        close(dst_fd);
                        unlink(dst); // Remove partial file on error
                        return -1;
                }
                total_written += bytes_written;

                // Sanity check: make sure we're not writing more than expected
                if (total_written > src_stat.st_size) {
                        close(src_fd);
                        close(dst_fd);
                        unlink(dst);
                        return -1;
                }
        }

        if (bytes_read < 0) {
                close(src_fd);
                close(dst_fd);
                unlink(dst); // Remove partial file on error
                return -1;
        }

        // Sync to disk before closing
        if (fsync(dst_fd) != 0) {
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
