#include <ctype.h>
#include <errno.h>
#include <glib.h>
#include <math.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <time.h>
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
#include <stdlib.h>
#include <time.h>

int getRandomNumber(int min, int max)
{
        static int seeded = 0;
        if (!seeded)
        {
                srandom(time(NULL)); // Use srandom() to seed the random number generator
                seeded = 1;
        }

        return min + (random() % (max - min + 1)); // Use random() instead of rand()
}

#endif

void c_sleep(int milliseconds)
{
        struct timespec ts;
        ts.tv_sec = milliseconds / 1000; // Seconds part
        // Ensure that the nanoseconds part is computed safely, and no overflow happens
        ts.tv_nsec = (milliseconds % 1000) * 1000000;

        // Make sure that nanoseconds is within valid range
        if (ts.tv_nsec >= 1000000000)
        {
                ts.tv_sec += ts.tv_nsec / 1000000000;
                ts.tv_nsec %= 1000000000;
        }

        nanosleep(&ts, NULL);
}

void c_usleep(int microseconds)
{
        if (microseconds < 0 || microseconds > 100000000) // Max 100 seconds
        {
                return;
        }

        struct timespec ts;
        ts.tv_sec = microseconds / 1000000;
        ts.tv_nsec = (microseconds % 1000000) * 1000; // Convert remaining microseconds to nanoseconds

        nanosleep(&ts, NULL);
}

void c_strcpy(char *dest, const char *src, size_t dest_size)
{
        // Ensure the destination and source are valid, and dest_size is large enough to hold at least one byte
        if (dest && src && dest_size > 0)
        {
                // Calculate the length of the source string, limited by dest_size - 1 (for the null terminator)
                size_t src_length = strnlen(src, dest_size - 1);

                // Safely copy up to src_length bytes from src to dest
                memcpy(dest, src, src_length);

                // Null-terminate the destination string
                dest[src_length] = '\0';
        }
        else if (dest && dest_size > 0) // If source is NULL, we clear the destination buffer
        {
                dest[0] = '\0';
        }
}

char *stringToLower(const char *str)
{
        if (str == NULL)
        {
                return NULL;
        }

        size_t length = strnlen(str, MAXPATHLEN);

        return g_utf8_strdown(str, length);
}

char *stringToUpper(const char *str)
{
        if (str == NULL)
        {
                return NULL;
        }

        size_t length = strnlen(str, MAXPATHLEN);

        return g_utf8_strup(str, length);
}

char *c_strcasestr(const char *haystack, const char *needle, int maxScanLen)
{
        if (!haystack || !needle || maxScanLen <= 0)
                return NULL;

        size_t needleLen = strnlen(needle, maxScanLen);

        if (needleLen == 0)
                return (char *)haystack;

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

        regmatch_t pmatch[1];
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
                fprintf(stderr, "match_regex: Regex match failed");

                return 1;
        }
}

bool isValidUTF8(const char *str, size_t len)
{
        size_t i = 0;
        while (i < len)
        {
                unsigned char c = str[i];
                if (c <= 0x7F) // 1-byte ASCII character
                {
                        i++;
                }
                else if ((c & 0xE0) == 0xC0) // 2-byte sequence
                {
                        if (i + 1 >= len || (str[i + 1] & 0xC0) != 0x80)
                                return false;
                        i += 2;
                }
                else if ((c & 0xF0) == 0xE0) // 3-byte sequence
                {
                        if (i + 2 >= len || (str[i + 1] & 0xC0) != 0x80 || (str[i + 2] & 0xC0) != 0x80)
                                return false;
                        i += 3;
                }
                else if ((c & 0xF8) == 0xF0) // 4-byte sequence
                {
                        if (i + 3 >= len || (str[i + 1] & 0xC0) != 0x80 || (str[i + 2] & 0xC0) != 0x80 || (str[i + 3] & 0xC0) != 0x80)
                                return false;
                        i += 4;
                }
                else
                {
                        return false; // Invalid UTF-8
                }
        }
        return true;
}

void extractExtension(const char *filename, size_t ext_size, char *ext)
{
        if (!filename || !ext || ext_size == 0)
        {
                if (ext && ext_size > 0)
                        ext[0] = '\0';
                return;
        }

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
        size_t dot_pos = dot - filename + 1;

        // Copy the extension while checking for UTF-8 validity
        while (dot_pos + i < length && filename[dot_pos + i] != '\0' && j < ext_size - 1)
        {
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
                        else
                        {
                                break; // Invalid UTF-8 start byte
                        }
                }

                // Ensure we don't overflow the destination buffer
                if (j + char_size >= ext_size)
                        break;

                // Check if the character is valid UTF-8
                if (isValidUTF8(&filename[dot_pos + i], char_size))
                {
                        // Copy the character to the extension buffer
                        memcpy(ext + j, filename + dot_pos + i, char_size);
                        j += char_size;
                        i += char_size;
                }
                else
                {
                        break; // Invalid UTF-8, stop copying
                }
        }

        // Null-terminate the extension
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
        if (!str || maxLen <= 0)
        {
                return;
        }

        // Find start (skip leading whitespace)
        char *start = str;
        while (*start && isspace(*start))
        {
                start++;
        }

        // Handle case where string is all whitespace or empty
        size_t len = strnlen(start, maxLen - (start - str));
        if (len == 0)
        {
                str[0] = '\0';
                return;
        }

        // Find end (skip trailing whitespace)
        char *end = start + len - 1;
        while (end >= start && isspace(*end))
        {
                end--;
        }

        // Null terminate
        *(end + 1) = '\0';

        // Move trimmed string to beginning if needed
        if (start != str)
        {
                size_t trimmed_len = end - start + 1;
                memmove(str, start, trimmed_len + 1); // +1 for null terminator
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

bool isValidFilename(const char *filename)
{
        // Check for path traversal patterns
        if (strstr(filename, "..") != NULL)
        {
                return false;
        }

        // Check for path separators (works for UTF-8)
        if (strchr(filename, '/') != NULL || strchr(filename, '\\') != NULL)
        {
                return false;
        }

        // Don't allow starting with dot (hidden files)
        if (filename[0] == '.')
        {
                return false;
        }

        // Allow everything else (including UTF-8 Chinese characters)
        return true;
}

char *getFilePath(const char *filename)
{
        if (filename == NULL || !isValidFilename(filename))
        {
                return NULL;
        }

        // Also check it doesn't start with a dot (hidden files)
        if (filename[0] == '.')
        {
                return NULL;
        }

        char *configdir = getConfigPath();
        if (configdir == NULL)
        {
                return NULL;
        }

        size_t configdir_length = strnlen(configdir, MAXPATHLEN);
        size_t filename_length = strnlen(filename, MAXPATHLEN);

        size_t filepath_length = configdir_length + 1 + filename_length + 1;

        if (filepath_length > MAXPATHLEN)
        {
                free(configdir);
                return NULL;
        }

        char *filepath = (char *)malloc(filepath_length);
        if (filepath == NULL)
        {
                free(configdir);
                return NULL;
        }

        snprintf(filepath, filepath_length, "%s/%s", configdir, filename);

        free(configdir);
        return filepath;
}

void removeUnneededChars(char *str, int length)
{
        // Do not remove characters if filename only contains digits
        bool stringContainsLetters = false;
        for (int i = 0; str[i] != '\0'; i++)
        {
                if (!isdigit(str[i]))
                {
                        stringContainsLetters = true;
                }
        }
        if (!stringContainsLetters)
        {
                return;
        }

        for (int i = 0; i < 3 && str[i] != '\0' && str[i] != ' '; i++)
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
                        length--;
                }
        }

        // Remove hyphens and underscores from filename
        for (int i = 0; str[i] != '\0'; i++)
        {
                // Only remove if there are no spaces around
                if ((str[i] == '-' || str[i] == '_') && (i > 0 && i < length && str[i - 1] != ' ' && str[i + 1] != ' '))
                {
                        str[i] = ' ';
                }
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

float getFloat(const char *str)
{
        char *endptr;
        float value = strtof(str, &endptr);

        if (str == endptr)
        {
                return 0.0f;
        }

        if (isnan(value) || isinf(value) || value < -FLT_MAX || value > FLT_MAX)
        {
                return 0.0f;
        }

        return value;
}
