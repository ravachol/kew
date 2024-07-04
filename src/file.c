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
        size_t pathLength = strlen(path);

        // Find the last occurrence of the directory separator character ('/' or '\')
        const char *lastSeparator = strrchr(path, '/');
        const char *lastBackslash = strrchr(path, '\\');

        // Determine the last occurrence of the directory separator
        const char *lastDirectorySeparator = lastSeparator > lastBackslash ? lastSeparator : lastBackslash;

        if (lastDirectorySeparator != NULL)
        {
                // Calculate the length of the directory path
                size_t directoryLength = lastDirectorySeparator - path + 1;

                if (directoryLength < pathLength)
                {
                        // Copy the directory path into the 'directory' buffer
                        strncpy(directory, path, directoryLength);
                        directory[directoryLength] = '\0';
                }
                else
                {
                        // The provided path is already a directory
                        c_strcpy(directory, sizeof(directory), path);
                }
        }
        else
        {
                // No directory separator found, return an empty string
                directory[0] = '\0';
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
int walker(const char *startPath, const char *searching, char *result,
           const char *allowedExtensions, enum SearchType searchType, bool exactSearch)
{
        DIR *d;
        struct dirent *dir;
        struct stat file_stat;
        char ext[6]; // +1 for null-terminator
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
                        if (((exactSearch && (strcasecmp(dir->d_name, searching) == 0)) || (!exactSearch && c_strcasestr(dir->d_name, searching) != NULL)) &&
                            (searchType != FileOnly) && (searchType != SearchPlayList))
                        {
                                char *curDir = getcwd(NULL, 0);
                                snprintf(result, MAXPATHLEN, "%s/%s", curDir, dir->d_name);
                                free(curDir);
                                copyresult = true;
                                break;
                        }
                        else
                        {
                                if (chdir(dir->d_name) == -1)
                                {
                                        fprintf(stderr, "Failed to change directory: %s\n", dir->d_name);
                                        continue;
                                }
                                if (walker(NULL, searching, result, allowedExtensions, searchType, exactSearch) == 0)
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
                        if (strlen(filename) <= 4)
                        {
                                continue;
                        }

                        extractExtension(filename, sizeof(ext) - 1, ext);
                        if (match_regex(&regex, ext) != 0)
                        {
                                continue;
                        }

                        if ((exactSearch && (strcasecmp(dir->d_name, searching) == 0)) || (!exactSearch && c_strcasestr(dir->d_name, searching) != NULL))
                        {
                                char *curDir = getcwd(NULL, 0);
                                snprintf(result, MAXPATHLEN, "%s/%s", curDir, dir->d_name);
                                copyresult = true;
                                free(curDir);
                                break;
                        }
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

                size_t homeDirLen = strlen(homeDir);
                size_t inputPathLen = strlen(inputPath);

                if (homeDirLen + inputPathLen >= MAXPATHLEN)
                {
                        return -1; // Expanded path exceeds maximum length
                }

                strcpy(expandedPath, homeDir);
                strcat(expandedPath, inputPath);
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

int removeDirectory(const char *path)
{
        struct stat st;

        // Check if path exists
        if (stat(path, &st) != 0)
                return -1; // Path does not exist

        // Check if it is a directory
        if (!S_ISDIR(st.st_mode))
                return -1; // Path exists but is not a directory

        DIR *dir = opendir(path);

        if (dir == NULL)
                return -1; // Failed to open directory

        struct dirent *entry;
        char filePath[MAXPATHLEN];

        // Remove all entries in the directory
        while ((entry = readdir(dir)) != NULL)
        {
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                        continue; // Skip current and parent directories

                snprintf(filePath, sizeof(filePath), "%s/%s", path, entry->d_name);

                if (entry->d_type == DT_DIR)
                        removeDirectory(filePath); // Recursively remove subdirectories
                else
                        remove(filePath); // Remove regular file
        }

        closedir(dir);

        // Remove the directory itself
        if (rmdir(path) == 0)
                return 0; // Directory removed successfully

        return -1; // Failed to remove directory
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
        if (tempDir == NULL)
        {
#ifdef __APPLE__
                tempDir = "/tmp";
#else
                tempDir = "/tmp";
#endif
        }

        return (startsWith(path, tempDir));
}

void deleteTempDir()
{
        const char *tempDir = getenv("TMPDIR");
        if (tempDir == NULL)
        {
#ifdef __APPLE__
                tempDir = "/tmp";
#else
                tempDir = "/tmp";
#endif
        }
        else
        {
        }
        char dirPath[MAXPATHLEN];
        struct passwd *pw = getpwuid(getuid());
        const char *username = pw->pw_name;
        snprintf(dirPath, MAXPATHLEN, "%s/kew/%s", tempDir, username);
        removeDirectory(dirPath);
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
        if (tempDir == NULL)
        {
                tempDir = "/tmp";
        }

        char dirPath[MAXPATHLEN];
        struct passwd *pw = getpwuid(getuid());
        const char *username = pw->pw_name;
        snprintf(dirPath, MAXPATHLEN, "%s/kew", tempDir);
        createDirectory(dirPath);
        snprintf(dirPath, MAXPATHLEN, "%s/kew/%s", tempDir, username);
        createDirectory(dirPath);

        char randomString[7];

        for (int i = 0; i < 6; ++i)
        {
                randomString[i] = 'a' + rand() % 26;
        }
        randomString[6] = '\0';

        snprintf(filePath, MAXPATHLEN + 7, "%s/%s%.6s%s", dirPath, prefix, randomString, suffix);
}
