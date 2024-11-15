#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include "file.h"

/*

file.c

 This file should contain only simple utility functions related to files and directories.
 They should work independently and be as decoupled from the rest of the application as possible.

*/

void getDirectoryFromPath(const char *path, char *directory)
{
    size_t path_length = strnlen(path, MAXPATHLEN);
    char tempPath[path_length + 1];
    c_strcpy(tempPath, path, sizeof(tempPath));

    char *dir = dirname(tempPath);

    // Copy directory name to the output buffer
    snprintf(directory, MAXPATHLEN, "%s", dir);

    size_t directory_length = strnlen(directory, MAXPATHLEN);
    if (directory[directory_length - 1] != '/' && directory_length + 1 < MAXPATHLEN)
    {
        // Use snprintf to append '/' at the end of directory
        snprintf(directory + directory_length, MAXPATHLEN - directory_length, "/");
    }
}

int existsFile(const char *fname)
{
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
           const char *allowedExtensions, enum SearchType searchType, bool exactSearch)
{
        DIR *d;
        struct dirent *dir;
        struct stat file_stat;
        char ext[100]; // +1 for null-terminator
        regex_t regex;
        int ret = regcomp(&regex, allowedExtensions, REG_EXTENDED);
        if (ret != 0)
        {
                return -1;
        }

        bool copyresult = false;

        if (startPath != NULL)
        {
                d = opendir(startPath);
                if (d == NULL)
                {
                        fprintf(stderr, "Failed to open directory.\n");
                        return 0;
                }
                int chdirResult = chdir(startPath);

                if (chdirResult != 0)
                {
                        fprintf(stderr, "Failed to change directory: %s\n", startPath);
                        return 0;
                }
        }
        else
        {
                d = opendir(".");
                if (d == NULL)
                {
                        fprintf(stderr, "Failed to open current directory.\n");
                        return 0;
                }
        }

        while ((dir = readdir(d)))
        {
                if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
                {
                        continue;
                }

                char entryPath[MAXPATHLEN];
                char *currentDir = getcwd(NULL, 0);
                snprintf(entryPath, sizeof(entryPath), "%s/%s", currentDir, dir->d_name);
                free(currentDir);

                if (stat(entryPath, &file_stat) != 0)
                {
                        continue;
                }

                if (S_ISDIR(file_stat.st_mode))
                {
                        char *name = g_utf8_casefold(dir->d_name, -1);

                        if (((exactSearch && (strcasecmp(name, lowCaseSearching) == 0)) || (!exactSearch && c_strcasestr(name, lowCaseSearching, MAXPATHLEN) != NULL)) &&
                            (searchType != FileOnly) && (searchType != SearchPlayList))
                        {
                                char *curDir = getcwd(NULL, 0);
                                snprintf(result, MAXPATHLEN, "%s/%s", curDir, dir->d_name);
                                free(curDir);
                                free(name);
                                copyresult = true;
                                break;
                        }
                        else
                        {
                                free(name);

                                if (chdir(dir->d_name) == -1)
                                {
                                        fprintf(stderr, "Failed to change directory: %s\n", dir->d_name);
                                        continue;
                                }
                                if (walker(NULL, lowCaseSearching, result, allowedExtensions, searchType, exactSearch) == 0)
                                {
                                        copyresult = true;
                                        break;
                                }
                                if (chdir("..") == -1)
                                {
                                        fprintf(stderr, "Failed to change directory to parent.\n");
                                        break;
                                }
                        }
                }
                else
                {
                        if (searchType == DirOnly)
                        {
                                continue;
                        }

                        char *filename = dir->d_name;
                        if (strnlen(filename, 256) <= 4)
                        {
                                continue;
                        }

                        extractExtension(filename, sizeof(ext) - 1, ext);
                        if (match_regex(&regex, ext) != 0)
                        {
                                continue;
                        }

                        char *name = g_utf8_casefold(dir->d_name, -1);

                        if ((exactSearch && (strcasecmp(name, lowCaseSearching) == 0)) || (!exactSearch && c_strcasestr(name, lowCaseSearching, MAXPATHLEN) != NULL))
                        {
                                char *curDir = getcwd(NULL, 0);
                                snprintf(result, MAXPATHLEN, "%s/%s", curDir, dir->d_name);
                                copyresult = true;
                                free(curDir);
                                free(name);
                                break;
                        }
                        free(name);
                }
        }
        closedir(d);
        regfree(&regex);

        return copyresult ? 0 : 1;
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
                                struct passwd *pw = getpwnam(username);
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
                                struct passwd *pw = getpwuid(getuid());

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
        const char *tempDir = getenv("TMPDIR");
        if (tempDir == NULL || strnlen(tempDir, PATH_MAX) >= PATH_MAX)
        {
                tempDir = "/tmp";
        }

        return (pathStartsWith(path, tempDir));
}

bool checkFileBelowMaxSize(const char *filePath, int maxSize)
{
        struct stat st;

        if (stat(filePath, &st) == 0)
        {
                return (st.st_size <= maxSize);
        }

        perror("stat failed");

        return false;
}

void generateTempFilePath(char *filePath, const char *prefix, const char *suffix)
{
        const char *tempDir = getenv("TMPDIR");
        if (tempDir == NULL || strnlen(tempDir, PATH_MAX) >= PATH_MAX)
        {
                tempDir = "/tmp";
        }

        struct passwd *pw = getpwuid(getuid());
        const char *username = pw ? pw->pw_name : "unknown";

        char dirPath[MAXPATHLEN];
        snprintf(dirPath, sizeof(dirPath), "%s/kew", tempDir);
        createDirectory(dirPath);
        snprintf(dirPath, sizeof(dirPath), "%s/kew/%s", tempDir, username);
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
