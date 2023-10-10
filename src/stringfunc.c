#include "stringfunc.h"
#include <stdbool.h>

/*

stringfunc.c

 This file should contain only simple utility functions related to strings. 
 They should work independently and be as decoupled from the application as possible.

*/

char *stringToLower(char *str)
{
    for (int i = 0; str[i] != '\0'; i++)
    {
        str[i] = tolower(str[i]);
    }
    return str;
}

void replaceChr(char *str, char toReplace, char replacement)
{
    if (str == NULL)
    {
        return;
    }
    size_t length = strlen(str);

    for (size_t i = 0; i < length; i++)
    {
        if (str[i] == toReplace)
        {
            str[i] = replacement;
        }
    }
}

char *c_strcasestr(const char *haystack, const char *needle)
{
    if (!haystack || !needle)
        return NULL;

    size_t needleLen = strlen(needle);
    size_t haystackLen = strlen(haystack);

    if (needleLen > haystackLen)
        return NULL;

    for (size_t i = 0; i <= haystackLen - needleLen; i++)
    {
        size_t j;
        for (j = 0; j < needleLen; j++)
        {
            if (tolower(haystack[i + j]) != tolower(needle[j]))
                break;
        }
        if (j == needleLen)
            return (char *)(haystack + i);
    }

    return NULL;
}

int match_regex(regex_t *regex, const char *ext)
{
    if (regex == NULL || ext == NULL)
    {
        fprintf(stderr, "Invalid arguments\n");
        return 1;
    }
    regmatch_t pmatch[1]; // Array to store match results
    int ret = regexec(regex, ext, 1, pmatch, 0);
    if (ret == REG_NOMATCH)
    {
        return 1;
    }
    else if (ret == 0)
    {
        return 0;
    }
    else
    {
        char errorBuf[100];
        regerror(ret, regex, errorBuf, sizeof(errorBuf));
        fprintf(stderr, "Regex match failed: %s\n", errorBuf);
        return 1;
    }
}

void extractExtension(const char *filename, size_t numChars, char *ext)
{
    size_t length = strlen(filename);
    size_t copyChars = length < numChars ? length : numChars;

    const char *extStart = filename + length - copyChars;
    strncpy(ext, extStart, copyChars);
    ext[copyChars] = '\0';
}

int endsWith(const char *str, const char *suffix)
{
    size_t strLength = strlen(str);
    size_t suffixLength = strlen(suffix);

    if (suffixLength > strLength)
    {
        return 0;
    }

    const char *strSuffix = str + (strLength - suffixLength);
    return strcmp(strSuffix, suffix) == 0;
}

bool containsCharacter(const char *str, const char character)
{
    while (*str != '\0')
    {
        if (*str == character)
        {
            return true;
        }
        str++;
    }
    return false;
}

void trim(char *str)
{
    char *start = str;
    while (*start && isspace(*start))
    {
        start++;
    }
    char *end = str + strlen(str) - 1;
    while (end > start && isspace(*end))
    {
        end--;
    }
    *(end + 1) = '\0';

    if (str != start)
    {
        memmove(str, start, end - start + 2);
    }
}

void removeSubstring(char *str, const char *substr)
{
    int len = strlen(substr);
    char *p = strstr(str, substr);

    if (p != NULL)
    {
        memmove(p, p + len, strlen(p + len) + 1);
    }
}
