#include "utils.h"

/*

 utils.c

 Utility functions for instance for replacing some standard functions with safer alternatives.

*/

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <stdlib.h> // For arc4random
#include <stdint.h> // For uint32_t

uint32_t arc4random_uniform(uint32_t upper_bound);

int getRandomNumber(int min, int max)
{
        return min + arc4random_uniform(max - min + 1);
}

#else
#include <sys/random.h> // For getrandom

int getRandomNumber(int min, int max)
{
        unsigned int random_value;
        if (getrandom(&random_value, sizeof(random_value), 0) != sizeof(random_value))
        {
                return min;
        }
        return min + (random_value % (max - min + 1));
}

#endif

void c_sleep(int milliseconds)
{
        struct timespec ts;
        ts.tv_sec = milliseconds / 1000;
        ts.tv_nsec = milliseconds % 1000 * 1000000;
        nanosleep(&ts, NULL);
}

void c_usleep(int microseconds)
{
        struct timespec ts;
        ts.tv_sec = microseconds / 1000000;
        ts.tv_nsec = microseconds % 1000;
        nanosleep(&ts, NULL);
}

void c_strcpy(char *dest, const char *src, size_t dest_size)
{
        if (dest && dest_size > 0 && src)
        {
                size_t src_length = strnlen(src, dest_size - 1);
                memcpy(dest, src, src_length);
                dest[src_length] = '\0';
        }
}

char *stringToLower(const char *str)
{
        return g_utf8_strdown(str, -1);
}

char *stringToUpper(const char *str)
{
        return g_utf8_strup(str, -1);
}

char *c_strcasestr(const char *haystack, const char *needle, int maxScanLen)
{
        if (!haystack || !needle)
                return NULL;

        size_t needleLen = strnlen(needle, maxScanLen);
        size_t haystackLen = strnlen(haystack, maxScanLen);

        if (needleLen > haystackLen)
                return NULL;

        for (size_t i = 0; i <= haystackLen - needleLen; i++)
        {
                if (strncasecmp(&haystack[i], needle, needleLen) == 0)
                {
                        return (char *)(haystack + i);
                }
        }

        return NULL;
}

int match_regex(const regex_t *regex, const char *ext)
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

void extractExtension(const char *filename, size_t ext_size, char *ext)
{
        size_t length = strnlen(filename, MAXPATHLEN);

        // Find the last '.' character in the filename
        const char *dot = NULL;
        for (size_t i = 0; i < length; i++)
        {
                if (filename[i] == '.')
                {
                        dot = &filename[i];
                }
        }

        // If no dot was found, there's no extension
        if (!dot || dot == filename + length - 1)
        {
                ext[0] = '\0'; // No extension found
                return;
        }

        size_t i = 0, j = 0;
        while (dot[i + 1] != '\0' && j < ext_size - 1)
        {
                unsigned char c = dot[i + 1];

                // Determine the number of bytes for the current UTF-8 character
                size_t charSize;
                if (c < 0x80)
                {
                        charSize = 1; // 1-byte character (ASCII)
                }
                else if ((c & 0xE0) == 0xC0)
                {
                        charSize = 2; // 2-byte character
                }
                else if ((c & 0xF0) == 0xE0)
                {
                        charSize = 3; // 3-byte character
                }
                else if ((c & 0xF8) == 0xF0)
                {
                        charSize = 4; // 4-byte character
                }
                else
                {
                        // Invalid UTF-8 byte sequence, stop copying
                        break;
                }

                // Make sure we don't copy past the buffer
                if (j + charSize >= ext_size)
                {
                        break;
                }

                // Copy the UTF-8 character
                memcpy(ext + j, dot + 1 + i, charSize);
                j += charSize;
                i += charSize;
        }

        // Null-terminate the result
        ext[j] = '\0';
}

int pathEndsWith(const char *str, const char *suffix)
{
        size_t length = strnlen(str, MAXPATHLEN);
        size_t suffixLength = strnlen(suffix, MAXPATHLEN);

        if (suffixLength > length)
        {
                return 0;
        }

        const char *strSuffix = str + (length - suffixLength);
        return strcmp(strSuffix, suffix) == 0;
}

int pathStartsWith(const char *str, const char *prefix)
{
        size_t length = strnlen(str, MAXPATHLEN);
        size_t prefixLength = strnlen(prefix, MAXPATHLEN);

        if (prefixLength > length)
        {
                return 0;
        }

        return strncmp(str, prefix, prefixLength) == 0;
}

void trim(char *str, int maxLen)
{
        char *start = str;
        while (*start && isspace(*start))
        {
                start++;
        }
        char *end = str + strnlen(str, maxLen) - 1;
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

const char *getHomePath(void)
{
        struct passwd *pw = getpwuid(getuid());
        if (pw && pw->pw_dir)
        {
                return pw->pw_dir;
        }
        return NULL;
}

char *getConfigPath(void)
{
        char *configPath = malloc(MAXPATHLEN);
        if (!configPath)
                return NULL;

        const char *xdgConfig = getenv("XDG_CONFIG_HOME");

        if (xdgConfig)
        {
                snprintf(configPath, MAXPATHLEN, "%s/kew", xdgConfig);
        }
        else
        {
                const char *home = getHomePath();
                if (home)
                {
#ifdef __APPLE__
                        snprintf(configPath, MAXPATHLEN, "%s/Library/Preferences/kew", home);
#else
                        snprintf(configPath, MAXPATHLEN, "%s/.config/kew", home);
#endif
                }
                else
                {
                        struct passwd *pw = getpwuid(getuid());
                        if (pw)
                        {
#ifdef __APPLE__
                                snprintf(configPath, MAXPATHLEN, "%s/Library/Preferences/kew", pw->pw_dir);
#else
                                snprintf(configPath, MAXPATHLEN, "%s/.config/kew", pw->pw_dir);
#endif
                        }
                        else
                        {
                                free(configPath);
                                return NULL;
                        }
                }
        }

        return configPath;
}

void removeUnneededChars(char *str)
{
        // Do not remove characters if filename only contains digits
        int i = 0;
        bool stringContainsLetters = false;
        while (str[i] != '\0')
        {
                if (!isdigit(str[i]))
                {
                        stringContainsLetters = true;
                }
                i++;
        }
        if (!stringContainsLetters)
        {
                return;
        }

        for (i = 0; i < 3 && str[i] != '\0' && str[i] != ' '; i++)
        {
                if (isdigit(str[i]) || str[i] == '.' || str[i] == '-' || str[i] == ' ')
                {
                        int j;
                        for (j = i; str[j] != '\0'; j++)
                        {
                                str[j] = str[j + 1];
                        }
                        str[j] = '\0';
                        i--; // Decrement i to re-check the current index
                }
        }
        i = 0;
        while (str[i] != '\0')
        {
                if (str[i] == '-' || str[i] == '_')
                {
                        str[i] = ' ';
                }
                i++;
        }
}

void shortenString(char *str, size_t maxLength)
{
        size_t length = strnlen(str, maxLength + 2);

        if (length > maxLength)
        {
                str[maxLength] = '\0';
        }
}

void printBlankSpaces(int numSpaces)
{
        if (numSpaces < 1)
                return;
        printf("%*s", numSpaces, " ");
}

int naturalCompare(const char *a, const char *b)
{
        while (*a && *b)
        {
                if (isdigit(*a) && isdigit(*b))
                {
                        // Compare numerically
                        char *endA, *endB;
                        long numA = strtol(a, &endA, 10);
                        long numB = strtol(b, &endB, 10);

                        if (numA != numB)
                        {
                                return numA - numB;
                        }

                        // Move pointers past the numeric part
                        a = endA;
                        b = endB;
                }
                else
                {
                        if (*a != *b)
                        {
                                return *a - *b;
                        }

                        a++;
                        b++;
                }
        }

        // If all parts so far are equal, shorter string should come first
        return *a - *b;
}

int compareLibEntries(const struct dirent **a, const struct dirent **b)
{
        // All strings need to be uppercased or already uppercased characters will come before all lower-case ones
        char *nameA = stringToUpper((*a)->d_name);
        char *nameB = stringToUpper((*b)->d_name);

        if (nameA[0] == '_' && nameB[0] != '_')
        {
                free(nameA);
                free(nameB);
                return 1;
        }
        else if (nameA[0] != '_' && nameB[0] == '_')
        {
                free(nameA);
                free(nameB);
                return -1;
        }

        int result = naturalCompare(nameA, nameB);

        free(nameA);
        free(nameB);

        return result;
}

int compareLibEntriesReversed(const struct dirent **a, const struct dirent **b)
{
        int result = compareLibEntries(a, b);

        return -result;
}

int getNumber(const char *str)
{
        char *endptr;
        long value = strtol(str, &endptr, 10);

        if (value < INT_MIN || value > INT_MAX)
        {
                return 0;
        }

        return (int)value;
}
