#include "utils.h"
/*

 utils.c

 Utility functions for instance for replacing some standard functions with safer alternatives.

*/
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

void c_strcpy(char *dest, size_t dest_size, const char *src)
{
        if (dest && dest_size > 0 && src)
        {
                size_t src_length = strlen(src);
                if (src_length < dest_size)
                {
                        strncpy(dest, src, dest_size);
                }
                else
                {
                        strncpy(dest, src, dest_size - 1);
                        dest[dest_size - 1] = '\0';
                }
        }
}

char *stringToLower(char *str)
{
        for (int i = 0; str[i] != '\0'; i++)
        {
                str[i] = tolower(str[i]);
        }
        return str;
}

char *stringToUpper(char *str)
{
        for (int i = 0; str[i] != '\0'; i++)
        {
                str[i] = toupper(str[i]);
        }
        return str;
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

int startsWith(const char *str, const char *prefix)
{
        size_t strLength = strlen(str);
        size_t prefixLength = strlen(prefix);

        if (prefixLength > strLength)
        {
                return 0;
        }

        return strncmp(str, prefix, prefixLength) == 0;
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

const char *getHomePath()
{
        const char *xdgHome = getenv("XDG_HOME");
        if (xdgHome == NULL)
        {
                xdgHome = getenv("HOME");
                if (xdgHome == NULL)
                {
                        struct passwd *pw = getpwuid(getuid());
                        if (pw != NULL)
                        {
                                char *home = (char *)malloc(MAXPATHLEN);
                                strcpy(home, pw->pw_dir);
                                return home;
                        }
                }
        }
        return xdgHome;
}

char *getConfigPathOld()
{
        char *configPath = malloc(MAXPATHLEN);
        if (!configPath)
                return NULL;

        const char *xdgConfig = getenv("XDG_CONFIG_HOME");
        if (xdgConfig)
        {
                snprintf(configPath, MAXPATHLEN, "%s", xdgConfig);
        }
        else
        {
                const char *home = getHomePath();
                if (home)
                {
                        snprintf(configPath, MAXPATHLEN, "%s/.config", home);
                }
                else
                {
                        struct passwd *pw = getpwuid(getuid());
                        if (pw)
                        {
                                snprintf(configPath, MAXPATHLEN, "%s", pw->pw_dir);
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

char *getConfigPath()
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
                        snprintf(configPath, MAXPATHLEN, "%s/.config/kew", home);
                }
                else
                {
                        struct passwd *pw = getpwuid(getuid());
                        if (pw)
                        {
                                snprintf(configPath, MAXPATHLEN, "%s/.config/kew", pw->pw_dir);
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

int moveConfigFiles()
{
        char *oldPath = getConfigPathOld();
        char *newPath = getConfigPath();
        if (!oldPath || !newPath)
        {

                free(oldPath);
                free(newPath);
                return -1;
        }

        struct stat st = {0};
        if (stat(newPath, &st) == -1)
        {
                mkdir(newPath, 0700);
        }

        char oldFileLibrary[MAXPATHLEN];
        char newFileLibrary[MAXPATHLEN];
        char oldFileRc[MAXPATHLEN];
        char newFileRc[MAXPATHLEN];

        snprintf(oldFileLibrary, MAXPATHLEN, "%s/kewlibrary", oldPath);
        snprintf(newFileLibrary, MAXPATHLEN, "%s/kewlibrary", newPath);
        snprintf(oldFileRc, MAXPATHLEN, "%s/kewrc", oldPath);
        snprintf(newFileRc, MAXPATHLEN, "%s/kewrc", newPath);

        if (access(oldFileLibrary, F_OK) == 0)
        {
                rename(oldFileLibrary, newFileLibrary);
        }
        if (access(oldFileRc, F_OK) == 0)
        {
                rename(oldFileRc, newFileRc);
        }

        free(oldPath);
        free(newPath);

        return 0;
}

void removeUnneededChars(char *str)
{
        if (strlen(str) < 6)
        {
                return;
        }

        int i;
        for (i = 0; i < 3 && str[i] != '\0' && str[i] != ' '; i++)
        {
                if (isdigit(str[i]) || str[i] == '-' || str[i] == ' ')
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
        size_t length = strlen(str);

        if (length > maxLength)
        {
                str[maxLength] = '\0';
        }
}

void printBlankSpaces(int numSpaces) {
    printf("%*s", numSpaces, " ");
}
