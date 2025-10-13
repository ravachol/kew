/**
 * @file utils.[h]
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

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

int getRandomNumber(int min, int max);
void c_sleep(int milliseconds);
void c_usleep(int microseconds);
void c_strcpy(char *dest, const char *src, size_t dest_size);
char *stringToUpper(const char *str);
char *stringToLower(const char *str);
char *utf8_strstr(const char *haystack, const char *needle);
char *c_strcasestr(const char *haystack, const char *needle, int maxScanLen);
int match_regex(const regex_t *regex, const char *ext);
gint64 getLengthInMicroSec(double duration);
void extractExtension(const char *filename, size_t numChars, char *ext);
int pathEndsWith(const char *str, const char *suffix);
int pathStartsWith(const char *str, const char *prefix);
void trim(char *str, int maxLen);
const char *getHomePath(void);
char *getConfigPath(void);
char *getPrefsPath(void);
void removeUnneededChars(char *str, int length);
void shortenString(char *str, size_t maxLength);
void printBlankSpaces(int numSpaces);
int getNumber(const char *str);
char *getFilePath(const char *filename);
float getFloat(const char *str);
int copyFile(const char *src, const char *dst);
int getNumberFromString(const char *str);

#endif
