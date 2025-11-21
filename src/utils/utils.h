/**
 * @file utils.h
 * @brief General-purpose utility functions and helpers.
 *
 * Provides miscellaneous helpers used across modules such as
 * string operations, math functions, or logging utilities.
 */

#ifndef UTILS_H
#define UTILS_H

#include <glib.h>
#include <regex.h>

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef __USE_POSIX
#define __USE_POSIX
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

int get_random_number(int min, int max);
int get_number(const char *str);
int copy_file(const char *src, const char *dst);
int get_number_from_string(const char *str);
int match_regex(const regex_t *regex, const char *ext);
int path_ends_with(const char *str, const char *suffix);
int path_starts_with(const char *str, const char *prefix);
char *get_file_path(const char *filename);
char *string_to_upper(const char *str);
char *string_to_lower(const char *str);
char *utf8_strstr(const char *haystack, const char *needle);
char *c_strcasestr(const char *haystack, const char *needle, int max_scan_len);
char *get_config_path(void);
char *get_prefs_path(void);
const char *get_home_path(void);
void c_sleep(int milliseconds);
void c_usleep(int microseconds);
void c_strcpy(char *dest, const char *src, size_t dest_size);
void extract_extension(const char *filename, size_t num_chars, char *ext);
void trim(char *str, int max_len);
void remove_unneeded_chars(char *str, int length);
void shorten_string(char *str, size_t max_length);
void print_blank_spaces(int num_spaces);
float get_float(const char *str);
gint64 get_length_in_micro_sec(double duration);

#endif
