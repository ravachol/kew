#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>

char* stringToLower(char* str);

char* strcasestr(const char* haystack, const char* needle);

int match_regex(regex_t* regex, const char* ext);

void extractExtension(const char* filename, size_t numChars, char* ext);

char* getDirectoryFromPath(const char* filepath);

int endsWith(const char* str, const char* suffix);