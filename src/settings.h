#include <pwd.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// saves the path to your music folder
int saveSettings(char *path, const char* settingsFile) 
{
    FILE *file;
    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;
    size_t len = 1024;

    // Try to open file
    file = fopen(strcat(strcat(strcpy((char*)malloc(strlen(homedir) + strlen("/") + strlen(settingsFile) + 1), homedir), "/"), settingsFile), "w");
    if (file != NULL) {
        // Check length before writing
        if (strlen(path) <= len) {
            fputs(path, file);
            fclose(file);
        }
    }
    // Return success
    return 0;
}

// reads the settings file, which contains the path to your music folder
int getsettings(char *path, int len, const char* settingsFile) 
{
    FILE *file;
    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;

    // Try to open file
    file = fopen(strcat(strcat(strcpy((char*)malloc(strlen(homedir) + strlen("/") + strlen(settingsFile) + 1), homedir), "/"), settingsFile), "r");
    if (file != NULL) {
        // Read lines into the buffer
        while (fgets(path, len, file) != NULL && len > 0) {
            // Remove trailing newline character if present
            size_t path_len = strlen(path);
            if (path_len > 0 && path[path_len - 1] == '\n') {
                path[path_len - 1] = '\0';
            }
        }
        fclose(file);
        return 0;
    } else {
        return -1;
    }
}