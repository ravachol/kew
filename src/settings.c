#include <stdbool.h>
#include <string.h>
#include "settings.h"
#include "stringfunc.h"

AppSettings settings;

const char PATH_SETTING_FILENAME[] = ".cue-settings";
const char SETTINGS_FILENAME[] = "cue.conf";

AppSettings constructAppSettings(KeyValuePair *pairs, int count)
{
    AppSettings settings;
    memset(&settings, 0, sizeof(settings));
    strncpy(settings.coverEnabled, "1", sizeof(settings.coverEnabled));

    if (pairs == NULL)
    {
        return settings;
    }

    for (int i = 0; i < count; i++)
    {
        KeyValuePair *pair = &pairs[i];

        if (strcmp(stringToLower(pair->key), "path") == 0)
        {
            strncpy(settings.path, pair->value, sizeof(settings.path));
        }
        else if (strcmp(stringToLower(pair->key), "coverenabled") == 0)
        {
            strncpy(settings.coverEnabled, pair->value, sizeof(settings.coverEnabled));
        }
        else if (strcmp(stringToLower(pair->key), "coverblocks") == 0)
        {
            strncpy(settings.coverBlocks, pair->value, sizeof(settings.coverBlocks));
        }
        else if (strcmp(stringToLower(pair->key), "visualizationenabled") == 0)
        {
            strncpy(settings.visualizationEnabled, pair->value, sizeof(settings.visualizationEnabled));
        }
        else if (strcmp(stringToLower(pair->key), "visualizationheight") == 0)
        {
            strncpy(settings.visualizationHeight, pair->value, sizeof(settings.visualizationHeight));
        }
    }

    return settings;
}

KeyValuePair *readKeyValuePairs(const char *file_path, int *count)
{
    FILE *file = fopen(file_path, "r");
    if (file == NULL)
    {
        // fprintf(stderr, "Error opening the settings file.\n");
        return NULL;
    }

    KeyValuePair *pairs = NULL;
    int pair_count = 0;

    char line[256];
    while (fgets(line, sizeof(line), file))
    {
        // Remove trailing newline character if present
        line[strcspn(line, "\n")] = '\0';

        char *delimiter = strchr(line, '=');
        if (delimiter != NULL)
        {
            *delimiter = '\0';
            char *value = delimiter + 1;

            pair_count++;
            pairs = realloc(pairs, pair_count * sizeof(KeyValuePair));
            KeyValuePair *current_pair = &pairs[pair_count - 1];

            current_pair->key = strdup(line);
            current_pair->value = strdup(value);
        }
    }

    fclose(file);

    *count = pair_count;
    return pairs;
}

void freeKeyValuePairs(KeyValuePair *pairs, int count)
{
    for (int i = 0; i < count; i++)
    {
        free(pairs[i].key);
        free(pairs[i].value);
    }

    free(pairs);
}

// saves the path to your music folder
int saveSettingsDeprecated(char *path, const char *settingsFile)
{
    FILE *file;
    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;
    size_t len = 1024;

    // Try to open file
    file = fopen(strcat(strcat(strcpy((char *)malloc(strlen(homedir) + strlen("/") + strlen(settingsFile) + 1), homedir), "/"), settingsFile), "w");
    if (file != NULL)
    {
        // Check length before writing
        if (strlen(path) <= len)
        {
            fputs(path, file);
            fclose(file);
        }
    }
    // Return success
    return 0;
}

// reads the settings file, which contains the path to your music folder
int getSettingsDeprecated(char *path, int len, const char *settingsFile)
{
    FILE *file;
    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;

    // Try to open file
    file = fopen(strcat(strcat(strcpy((char *)malloc(strlen(homedir) + strlen("/") + strlen(settingsFile) + 1), homedir), "/"), settingsFile), "r");
    if (file != NULL)
    {
        // Read lines into the buffer
        while (fgets(path, len, file) != NULL && len > 0)
        {
            // Remove trailing newline character if present
            size_t path_len = strlen(path);
            if (path_len > 0 && path[path_len - 1] == '\n')
            {
                path[path_len - 1] = '\0';
            }
        }
        fclose(file);
        return 0;
    }
    else
    {
        return -1;
    }
}

int getMusicLibraryPath(char *path)
{
    char expandedPath[MAXPATHLEN];

    if (path[0] != '\0' && path[0] != '\r')
    {
        if (expandPath(path, expandedPath) >= 0)
            strcpy(path, expandedPath);
        return 0;
    }
    getSettingsDeprecated(path, MAXPATHLEN, PATH_SETTING_FILENAME);

    if (path[0] == '\0') // if NULL, ie no path setting was found
    {
        struct passwd *pw = getpwuid(getuid());
        strcat(path, pw->pw_dir);
        strcat(path, "/Music/");
    }

    if (expandPath(path, expandedPath))
        strcpy(path, expandedPath);

    return 0;
}

void getConfig(const char *filename)
{
    int pair_count;
    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;
    const char *filepath = strcat(strcat(strcpy((char *)malloc(strlen(homedir) + strlen("/") + strlen(filename) + 1), homedir), "/"), filename);
    KeyValuePair *pairs = readKeyValuePairs(filepath, &pair_count);
    settings = constructAppSettings(pairs, pair_count);

    coverEnabled = (settings.coverEnabled[0] == '1');
    coverBlocks = (settings.coverBlocks[0] == '1');
    visualizationEnabled = (settings.visualizationEnabled[0] == '1');
    int temp = atoi(settings.visualizationHeight);
    if (temp > 0)
        visualizationHeight = temp;
    getMusicLibraryPath(settings.path);
}

void setConfig(AppSettings *settings, const char *filename)
{
    int pair_count = 4; // Number of key-value pairs in AppSettings

    // Create the file path
    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;

    char *filepath = (char *)malloc(strlen(homedir) + strlen("/") + strlen(filename) + 1);
    strcpy(filepath, homedir);
    strcat(filepath, "/");
    strcat(filepath, filename);

    // Open the file for writing
    FILE *file = fopen(filepath, "w");
    if (file == NULL)
    {
        fprintf(stderr, "Error opening file: %s\n", filepath);
        free(filepath);
        return;
    }

    // Set defaults if null
    if (settings->coverEnabled[0] == '\0')
        strcpy(settings->coverEnabled, "1");
    if (settings->coverBlocks[0] == '\0')
        strcpy(settings->coverBlocks, "1");
    if (settings->visualizationEnabled[0] == '\0')
        strcpy(settings->visualizationEnabled, "0");
    if (settings->visualizationHeight[0] == '\0')
    {
        sprintf(settings->visualizationHeight, "%d", visualizationHeight);
    }

    // Null-terminate the character arrays
    settings->path[MAXPATHLEN - 1] = '\0';
    settings->coverEnabled[1] = '\0';
    settings->coverBlocks[1] = '\0';
    settings->visualizationEnabled[1] = '\0';
    settings->visualizationHeight[5] = '\0';

    // Write the settings to the file
    fprintf(file, "path=%s\n", settings->path);
    fprintf(file, "coverEnabled=%s\n", settings->coverEnabled);
    fprintf(file, "coverBlocks=%s\n", settings->coverBlocks);
    fprintf(file, "visualizationEnabled=%s\n", settings->visualizationEnabled);
    fprintf(file, "visualizationHeight=%s\n", settings->visualizationHeight);

    // Close the file and free the allocated memory
    fclose(file);
    free(filepath);
}