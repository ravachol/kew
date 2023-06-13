#include <stdbool.h>
#include <string.h>
#include "settings.h"
#include "stringextensions.h"

AppSettings constructAppSettings(KeyValuePair* pairs, int count) 
{
    AppSettings settings;
    memset(&settings, 0, sizeof(settings));

    if (pairs == NULL)
    {
        return settings;
    }

    for (int i = 0; i < count; i++) {
        KeyValuePair* pair = &pairs[i];

        if (strcmp(stringToLower(pair->key), "path") == 0) {
            strncpy(settings.path, pair->value, sizeof(settings.path));
        } else if (strcmp(stringToLower(pair->key), "coverenabled") == 0) {
            strncpy(settings.coverEnabled, pair->value, sizeof(settings.coverEnabled));
        } else if (strcmp(stringToLower(pair->key), "coverblocks") == 0) {
            strncpy(settings.coverBlocks, pair->value, sizeof(settings.coverBlocks));
        } else if (strcmp(stringToLower(pair->key), "visualizationenabled") == 0) {
            strncpy(settings.visualizationEnabled, pair->value, sizeof(settings.visualizationEnabled));
        } else if (strcmp(stringToLower(pair->key), "visualizationheight") == 0) {
            strncpy(settings.visualizationHeight, pair->value, sizeof(settings.visualizationHeight));            
        }        
    }

    return settings;
}

KeyValuePair* readKeyValuePairs(const char* file_path, int* count) 
{
    FILE* file = fopen(file_path, "r");
    if (file == NULL) {
        //fprintf(stderr, "Error opening the settings file.\n");
        return NULL;
    }

    KeyValuePair* pairs = NULL;
    int pair_count = 0;

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Remove trailing newline character if present
        line[strcspn(line, "\n")] = '\0';

        char* delimiter = strchr(line, '=');
        if (delimiter != NULL) {
            *delimiter = '\0';
            char* value = delimiter + 1;

            pair_count++;
            pairs = realloc(pairs, pair_count * sizeof(KeyValuePair));
            KeyValuePair* current_pair = &pairs[pair_count - 1];

            current_pair->key = strdup(line);
            current_pair->value = strdup(value);
        }
    }

    fclose(file);

    *count = pair_count;
    return pairs;
}

void freeKeyValuePairs(KeyValuePair* pairs, int count) 
{
    for (int i = 0; i < count; i++) {
        free(pairs[i].key);
        free(pairs[i].value);
    }

    free(pairs);
}

// saves the path to your music folder
int saveSettingsDeprecated(char *path, const char* settingsFile) 
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
int getSettingsDeprecated(char *path, int len, const char* settingsFile) 
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