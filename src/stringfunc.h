#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>
#include <stdio.h>
#include <stdbool.h>

char *stringToLower(char *str);

void replaceChr(char *str, char toReplace, char replacement);

char *c_strcasestr(const char *haystack, const char *needle);

int match_regex(regex_t *regex, const char *ext);

void extractExtension(const char *filename, size_t numChars, char *ext);

int endsWith(const char *str, const char *suffix);

bool containsCharacter(const char *str, const char character);

void trim(char *str);

void removeSubstring(char *str, const char *substr);