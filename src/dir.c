#include "dir.h"

void getDirectoryFromPath(const char* path, char* directory) 
{
  size_t pathLength = strlen(path);
  
  // Find the last occurrence of the directory separator character ('/' or '\')
  const char* lastSeparator = strrchr(path, '/');
  const char* lastBackslash = strrchr(path, '\\');
  
  // Determine the last occurrence of the directory separator
  const char* lastDirectorySeparator = lastSeparator > lastBackslash ? lastSeparator : lastBackslash;
  
  if (lastDirectorySeparator != NULL) {
    // Calculate the length of the directory path
    size_t directoryLength = lastDirectorySeparator - path + 1;
    
    if (directoryLength < pathLength) {
      // Copy the directory path into the 'directory' buffer
      strncpy(directory, path, directoryLength);
      directory[directoryLength] = '\0';
    } else {
      // The provided path is already a directory
      strcpy(directory, path);
    }
  } else {
    // No directory separator found, return an empty string
    directory[0] = '\0';
  }
}

int tryOpen(const char* path) 
{
    DIR* dir = opendir(path);
    if (dir != NULL) {
        closedir(dir);
        return 1;
    }
    return 0;
}

int isDirectory(const char* path) 
{
    struct stat path_stat;
    if (stat(path, &path_stat) != 0) {
        return 0;  // Unable to get file information, assume not a directory
    }

    return S_ISDIR(path_stat.st_mode);
}

// Function to traverse a directory tree and search for a given file or directory
int walker(const char *startPath, const char *searching, char *result,
           const char *allowedExtensions, enum SearchType searchType) 
{
    DIR *d;
    struct dirent *dir;
    struct stat file_stat;
    char ext[6]; // +1 for null-terminator

    regex_t regex;
    int ret = regcomp(&regex, allowedExtensions, REG_EXTENDED);
    if (ret != 0) {
        return -1;
    }

    bool copyresult = false;

    if (startPath != NULL) {
        d = opendir(startPath);
        if (d == NULL) {
            fprintf(stderr, "Failed to open directory: %s\n", startPath);
            return 1;
        }
        chdir(startPath);
    } else {
        d = opendir(".");
        if (d == NULL) {
            fprintf(stderr, "Failed to open current directory.\n");
            return 1;
        }
    }

    while ((dir = readdir(d))) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
            continue;
        }

        char entryPath[PATH_MAX];
        snprintf(entryPath, sizeof(entryPath), "%s/%s", getcwd(NULL, 0), dir->d_name);

        if (stat(entryPath, &file_stat) != 0) {
            continue;
        }

        if (S_ISDIR(file_stat.st_mode)) {
            if ((strcasestr(dir->d_name, searching) != NULL) && (searchType != FileOnly)) {
                snprintf(result, MAXPATHLEN, "%s/%s", getcwd(NULL, 0), dir->d_name);
                copyresult = true;
                break;
            } else {          
                if (chdir(dir->d_name) == -1) {
                    fprintf(stderr, "Failed to change directory: %s\n", dir->d_name);
                    continue;
                }
                if (walker(NULL, searching, result, allowedExtensions, searchType) == 0) {
                    copyresult = true;
                    break;
                }
                if (chdir("..") == -1) {
                    fprintf(stderr, "Failed to change directory to parent.\n");
                    break;
                }
            }
        } else {
            if (searchType == DirOnly) {
                continue;
            }

            char *filename = dir->d_name;
            if (strlen(filename) <= 4) {
                continue;
            }

            extractExtension(filename, sizeof(ext) - 1, ext);
            if (match_regex(&regex, ext) != 0) {
                continue;
            }

            if (strcasestr(dir->d_name, searching) != NULL) {
                snprintf(result, MAXPATHLEN, "%s/%s", getcwd(NULL, 0), dir->d_name);
                copyresult = true;
                break;
            }
        }
    }

    closedir(d);
    return copyresult ? 0 : 1;
}

int expandPath(const char *inputPath, char *expandedPath)
{
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

        if (homeDirLen + inputPathLen >= PATH_MAX)
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