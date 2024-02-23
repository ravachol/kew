#ifndef UTILS_H
#define UTILS_H
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef __USE_POSIX
#define __USE_POSIX
#endif

#include <ctype.h>
#include <pwd.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

void c_sleep(int milliseconds);

void c_usleep(int microseconds);

void c_strcpy(char *dest, size_t dest_size, const char *src);

char *stringToUpper(char *str);

char *stringToLower(char *str);

char *c_strcasestr(const char *haystack, const char *needle);

int match_regex(regex_t *regex, const char *ext);

void extractExtension(const char *filename, size_t numChars, char *ext);

int endsWith(const char *str, const char *suffix);

int startsWith(const char *str, const char *prefix);

void trim(char *str);

const char *getHomePath();

char *getConfigPath();

#endif
