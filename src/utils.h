#ifndef UTILS_H
#define UTILS_H
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef __USE_POSIX
#define __USE_POSIX
#endif

#include <ctype.h>
#include <errno.h>
#include <glib.h>
#include <pwd.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <time.h>

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

void extractExtension(const char *filename, size_t numChars, char *ext);

int pathEndsWith(const char *str, const char *suffix);

int pathStartsWith(const char *str, const char *prefix);

void trim(char *str, int maxLen);

const char *getHomePath(void);

char *getConfigPath(void);

void removeUnneededChars(char *str, int length);

void shortenString(char *str, size_t maxLength);

void printBlankSpaces(int numSpaces);

int compareLibEntries(const struct dirent **a, const struct dirent **b);

int compareLibEntriesReversed(const struct dirent **a, const struct dirent **b);

int getNumber(const char *str);

#endif
