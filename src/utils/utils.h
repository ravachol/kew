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

/**
 * @brief Generates a random number between the given range.
 *
 * This function returns a random number between the specified minimum
 * and maximum (inclusive) values.
 *
 * @param min The minimum value of the range (inclusive).
 * @param max The maximum value of the range (inclusive).
 *
 * @return A random number between min and max.
 */
int get_random_number(int min, int max);

/**
 * @brief Converts a string to an integer.
 *
 * This function converts the provided string to an integer. It returns
 * the converted integer or 0 if the conversion fails.
 *
 * @param str The string to convert to an integer.
 *
 * @return The converted integer or 0 if conversion fails.
 */
int get_number(const char *str);

/**
 * @brief Copies a file from source to destination.
 *
 * This function copies the contents of a source file to a destination file.
 * It validates the input files and checks for errors during the copy process.
 *
 * @param src The source file path.
 * @param dst The destination file path.
 *
 * @return 0 on success, or a negative value on error.
 */
int copy_file(const char *src, const char *dst);

/**
 * @brief Extracts the numeric value from a string.
 *
 * This function extracts and returns the first number found in the string.
 *
 * @param str The string containing the number.
 *
 * @return The extracted number or 0 if no number is found.
 */
int get_number_from_string(const char *str);

/**
 * @brief Matches a regex against a string.
 *
 * This function matches the provided regular expression against the given
 * string and returns whether the match is successful.
 *
 * @param regex The compiled regex pattern.
 * @param ext The string to match against the regex.
 *
 * @return 0 if matched, 1 if not matched, or an error code on failure.
 */
int match_regex(const regex_t *regex, const char *ext);

/**
 * @brief Checks if the given string ends with the specified suffix.
 *
 * This function checks if the provided string ends with the given suffix.
 *
 * @param str The string to check.
 * @param suffix The suffix to check for.
 *
 * @return 1 if the string ends with the suffix, 0 otherwise.
 */
int path_ends_with(const char *str, const char *suffix);

/**
 * @brief Checks if the given string starts with the specified prefix.
 *
 * This function checks if the provided string starts with the given prefix.
 *
 * @param str The string to check.
 * @param prefix The prefix to check for.
 *
 * @return 1 if the string starts with the prefix, 0 otherwise.
 */
int path_starts_with(const char *str, const char *prefix);

/**
 * @brief Gets the file path for the given filename.
 *
 * This function retrieves the full file path for the given filename.
 *
 * @param filename The filename to retrieve the file path for.
 *
 * @return The full file path, or NULL if the filename is invalid.
 */
char *get_file_path(const char *filename);

/**
 * @brief Converts a string to uppercase.
 *
 * This function converts the provided string to uppercase.
 *
 * @param str The string to convert to uppercase.
 *
 * @return The uppercase version of the string.
 */
char *string_to_upper(const char *str);

/**
 * @brief Converts a string to lowercase.
 *
 * This function converts the provided string to lowercase.
 *
 * @param str The string to convert to lowercase.
 *
 * @return The lowercase version of the string.
 */
char *string_to_lower(const char *str);

/**
 * @brief Finds the first occurrence of a UTF-8 substring.
 *
 * This function searches for the first occurrence of a UTF-8 substring
 * within a larger string.
 *
 * @param haystack The string to search in.
 * @param needle The substring to search for.
 *
 * @return A pointer to the first occurrence of the needle, or NULL if not found.
 */
char *utf8_strstr(const char *haystack, const char *needle);

/**
 * @brief Case-insensitive string search with a length limit.
 *
 * This function performs a case-insensitive search for a substring
 * within a string, limiting the number of characters to scan.
 *
 * @param haystack The string to search in.
 * @param needle The substring to search for.
 * @param max_scan_len The maximum number of characters to scan.
 *
 * @return A pointer to the first occurrence of the needle, or NULL if not found.
 */
char *c_strcasestr(const char *haystack, const char *needle, int max_scan_len);

/**
 * @brief Retrieves the path to the configuration directory.
 *
 * This function retrieves the path to the configuration directory.
 *
 * @return The path to the configuration directory.
 */
char *get_config_path(void);

/**
 * @brief Retrieves the path to the preferences directory.
 *
 * This function retrieves the path to the preferences directory.
 *
 * @return The path to the preferences directory.
 */
char *get_prefs_path(void);

/**
 * @brief Retrieves the home directory path.
 *
 * This function retrieves the path to the user's home directory.
 *
 * @return The home directory path.
 */
const char *get_home_path(void);

/**
 * @brief Pauses the program for a specified number of milliseconds.
 *
 * This function pauses execution for the given number of milliseconds.
 *
 * @param milliseconds The number of milliseconds to sleep.
 */
void c_sleep(int milliseconds);

/**
 * @brief Pauses the program for a specified number of microseconds.
 *
 * This function pauses execution for the given number of microseconds.
 *
 * @param microseconds The number of microseconds to sleep.
 */
void c_usleep(int microseconds);

/**
 * @brief Copies a string with a specified buffer size.
 *
 * This function copies a string into a destination buffer, ensuring that
 * the buffer size is respected.
 *
 * @param dest The destination buffer.
 * @param src The source string to copy.
 * @param dest_size The size of the destination buffer.
 */
void c_strcpy(char *dest, const char *src, size_t dest_size);

/**
 * @brief Extracts the file extension from a filename.
 *
 * This function extracts the file extension from a given filename.
 *
 * @param filename The filename to extract the extension from.
 * @param ext_size The maximum size of the extension buffer.
 * @param ext The buffer to store the extracted extension.
 */
void extract_extension(const char *filename, size_t num_chars, char *ext);

/**
 * @brief Trims leading and trailing whitespace from a string.
 *
 * This function trims any leading and trailing whitespace characters
 * from the provided string.
 *
 * @param str The string to trim.
 * @param max_len The maximum length of the string to trim.
 */
void trim(char *str, size_t max_len);

/**
 * @brief Formats a filename by removing unnecessary characters.
 *
 * This function formats a filename by removing unnecessary track numbers
 * and characters.
 *
 * @param str The filename to format.
 */
void format_filename(char *str);

/**
 * @brief Shortens a string to a specified maximum length.
 *
 * This function shortens a string to fit within the specified maximum
 * length, truncating it if necessary.
 *
 * @param str The string to shorten.
 * @param max_length The maximum allowed length of the string.
 */
void shorten_string(char *str, size_t max_length);

/**
 * @brief Prints a specified number of blank spaces.
 *
 * This function prints a given number of blank spaces to the console.
 *
 * @param num_spaces The number of blank spaces to print.
 */
void print_blank_spaces(int num_spaces);

/**
 * @brief Converts a string to a float.
 *
 * This function converts the provided string to a floating-point value.
 *
 * @param str The string to convert.
 *
 * @return The converted float value.
 */
float get_float(const char *str);

/**
 * @brief Converts a duration in seconds to microseconds.
 *
 * This function converts the given duration in seconds (as a double) to
 * microseconds.
 *
 * @param duration The duration in seconds.
 *
 * @return The equivalent duration in microseconds.
 */
gint64 get_length_in_micro_sec(double duration);


#endif
