#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include <dirent.h>
#include <errno.h>
#include <glib.h>
#include <libgen.h>
#include <pwd.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "file.h"
#include "utils.h"

/*

file.c

 This file should contain only simple utility functions related to files and directories.
 They should work independently and be as decoupled from the rest of the application as possible.

*/

#define MAX_RECURSION_DEPTH 64

void getDirectoryFromPath(const char *path, char *directory)
{
        if (!path || !directory)
                return;

        size_t len = strnlen(path, MAXPATHLEN);

        char *tmp = malloc(len + 1);
        if (!tmp)
        {
                fprintf(stderr, "Out of memory while processing path\n");
                return;
        }

        memcpy(tmp, path, len + 1);

        // dirname() may modify the buffer, so we keep it in tmp
        char *dir = dirname(tmp);

        // Copy the result to the caller‑supplied buffer safely
        strlcpy(directory, dir, MAXPATHLEN);

        /// Ensure a trailing '/'
        size_t dlen = strnlen(directory, MAXPATHLEN);

        if (dlen > 0 && directory[dlen - 1] != '/' &&
            dlen + 1 < MAXPATHLEN)
        {
                directory[dlen] = '/';
                directory[dlen + 1] = '\0';
        }

        free(tmp);
}

int existsFile(const char *fname)
{
        if (fname == NULL || fname[0] == '\0')
                return -1;

        FILE *file;
        if ((file = fopen(fname, "r")))
        {
                fclose(file);
                return 1;
        }
        return -1;
}

int isDirectory(const char *path)
{
        DIR *dir = opendir(path);
        if (dir)
        {
                closedir(dir);
                return 1;
        }
        else
        {
                if (errno == ENOENT)
                {
                        return -1;
                }
                return 0;
        }
}

// Traverse a directory tree and search for a given file or directory
int walker(const char *startPath, const char *lowCaseSearching, char *result,
           const char *allowedExtensions, enum SearchType searchType, bool exactSearch, int depth)
{
    if (depth > MAX_RECURSION_DEPTH)
    {
        fprintf(stderr, "Maximum recursion depth exceeded\n");
        return 1;
    }

    if (!startPath || !lowCaseSearching || !result || !allowedExtensions)
    {
        fprintf(stderr, "Invalid arguments to walker\n");
        return 1;
    }

    struct stat path_stat;
    if (stat(startPath, &path_stat) != 0)
    {
        fprintf(stderr, "Cannot stat path '%s': %s\n", startPath, strerror(errno));
        return 1;
    }

    if (!S_ISDIR(path_stat.st_mode))
    {
        // Not a directory, stop here
        return 1;
    }

    DIR *d = opendir(startPath);
    if (!d)
    {
        fprintf(stderr, "Failed to open directory '%s': %s\n", startPath, strerror(errno));
        return 1;
    }

    regex_t regex;
    if (regcomp(&regex, allowedExtensions, REG_EXTENDED) != 0)
    {
        fprintf(stderr, "Failed to compile regex\n");
        closedir(d);
        return 1;
    }

    bool found = false;
    struct dirent *entry;
    char ext[100] = {0};

    while ((entry = readdir(d)) != NULL)
    {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Build full path for entry
        char entryPath[PATH_MAX];
        if (snprintf(entryPath, sizeof(entryPath), "%s/%s", startPath, entry->d_name) >= (int)sizeof(entryPath))
        {
            fprintf(stderr, "Path too long: %s/%s\n", startPath, entry->d_name);
            continue;
        }

        if (stat(entryPath, &path_stat) != 0)
        {
            // Can't stat, skip
            continue;
        }

        if (S_ISDIR(path_stat.st_mode))
        {
            // Directory handling
            char *foldedName = g_utf8_casefold(entry->d_name, -1);
            if (!foldedName)
                continue;

            bool nameMatch = exactSearch
                             ? (strcasecmp(foldedName, lowCaseSearching) == 0)
                             : (c_strcasestr(foldedName, lowCaseSearching, PATH_MAX) != NULL);

            free(foldedName);

            if (nameMatch && searchType != FileOnly && searchType != SearchPlayList)
            {
                strncpy(result, entryPath, PATH_MAX - 1);
                result[PATH_MAX - 1] = '\0';
                found = true;
                break;
            }

            // Recurse into subdirectory
            if (walker(entryPath, lowCaseSearching, result, allowedExtensions, searchType, exactSearch, depth + 1) == 0)
            {
                found = true;
                break;
            }
        }
        else
        {
            // File handling
            if (searchType == DirOnly)
                continue;

            if (strlen(entry->d_name) <= 4)
                continue;

            extractExtension(entry->d_name, sizeof(ext) - 1, ext);
            if (match_regex(&regex, ext) != 0)
                continue;

            char *foldedName = g_utf8_casefold(entry->d_name, -1);
            if (!foldedName)
                continue;

            bool nameMatch = exactSearch
                             ? (strcasecmp(foldedName, lowCaseSearching) == 0)
                             : (c_strcasestr(foldedName, lowCaseSearching, PATH_MAX) != NULL);

            free(foldedName);

            if (nameMatch)
            {
                strncpy(result, entryPath, PATH_MAX - 1);
                result[PATH_MAX - 1] = '\0';
                found = true;
                break;
            }
        }
    }

    regfree(&regex);
    closedir(d);

    return found ? 0 : 1;
}

int expandPath(const char *inputPath, char *expandedPath)
{
        if (inputPath[0] == '\0' || inputPath[0] == '\r')
                return -1;

        if (inputPath[0] == '~') // Check if inputPath starts with '~'
        {
                const char *homeDir;
                if (inputPath[1] == '/' || inputPath[1] == '\0') // Handle "~/"
                {
                        homeDir = getenv("HOME");
                        if (homeDir == NULL)
                        {
                                return -1; // Unable to retrieve home directory
                        }
                        inputPath++; // Skip '~' character
                }
                else // Handle "~username/"
                {
                        const char *username = inputPath + 1;
                        const char *slash = strchr(username, '/');
                        if (slash == NULL)
                        {
                                const struct passwd *pw = getpwnam(username);
                                if (pw == NULL)
                                {
                                        return -1; // Unable to retrieve user directory
                                }
                                homeDir = pw->pw_dir;
                                inputPath = ""; // Empty path component after '~username'
                        }
                        else
                        {
                                size_t usernameLen = slash - username;
                                const struct passwd *pw = getpwuid(getuid());

                                if (pw == NULL)
                                {
                                        return -1; // Unable to retrieve user directory
                                }
                                homeDir = pw->pw_dir;
                                inputPath += usernameLen + 1; // Skip '~username/' component
                        }
                }

                size_t homeDirLen = strnlen(homeDir, MAXPATHLEN);
                size_t inputPathLen = strnlen(inputPath, MAXPATHLEN);

                if (homeDirLen + inputPathLen >= MAXPATHLEN)
                {
                        return -1; // Expanded path exceeds maximum length
                }

                c_strcpy(expandedPath, homeDir, MAXPATHLEN);
                snprintf(expandedPath + homeDirLen, MAXPATHLEN - homeDirLen, "%s", inputPath);
        }
        else // Handle if path is not prefixed with '~'
        {
                if (realpath(inputPath, expandedPath) == NULL)
                {
                        return -1; // Unable to expand the path
                }
        }
        return 0; // Path expansion successful
}

int createDirectory(const char *path)
{
        struct stat st;

        // Check if directory already exists
        if (stat(path, &st) == 0)
        {
                if (S_ISDIR(st.st_mode))
                        return 0; // Directory already exists
                else
                        return -1; // Path exists but is not a directory
        }

        // Directory does not exist, so create it
        if (mkdir(path, 0700) == 0)
                return 1; // Directory created successfully

        return -1; // Failed to create directory
}

int deleteFile(const char *filePath)
{
        if (remove(filePath) == 0)
        {
                return 0;
        }
        else
        {
                return -1;
        }
}

int isInTempDir(const char *path)
{
        const char *tmpDir = getenv("TMPDIR");
        static char tmpdirBuf[PATH_MAX + 2];

        if (tmpDir == NULL || strnlen(tmpDir, PATH_MAX) >= PATH_MAX)
                tmpDir = "/tmp";

        size_t len = strlen(tmpDir);
        strncpy(tmpdirBuf, tmpDir, PATH_MAX);
        tmpdirBuf[PATH_MAX] = '\0';

        if (len == 0 || tmpdirBuf[len - 1] != '/')
        {
                tmpdirBuf[len] = '/';
                tmpdirBuf[len + 1] = '\0';
        }

        return pathStartsWith(path, tmpdirBuf);
}

void generateTempFilePath(char *filePath, const char *prefix, const char *suffix)
{
        const char *tmpDir = getenv("TMPDIR");
        if (tmpDir == NULL || strnlen(tmpDir, PATH_MAX) >= PATH_MAX)
        {
                tmpDir = "/tmp";
        }

        struct passwd *pw = getpwuid(getuid());
        const char *username = pw ? pw->pw_name : "unknown";

        char dirPath[MAXPATHLEN];
        snprintf(dirPath, sizeof(dirPath), "%s/kew", tmpDir);
        createDirectory(dirPath);
        snprintf(dirPath, sizeof(dirPath), "%s/kew/%s", tmpDir, username);
        createDirectory(dirPath);

        char randomString[7];
        for (int i = 0; i < 6; ++i)
        {
                randomString[i] = 'a' + rand() % 26;
        }
        randomString[6] = '\0';

        int written = snprintf(filePath, MAXPATHLEN, "%s/%s%.6s%s", dirPath, prefix, randomString, suffix);
        if (written < 0 || written >= MAXPATHLEN)
        {
                filePath[0] = '\0';
        }
}
