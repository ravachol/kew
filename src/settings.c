#include <stdbool.h>
#include <string.h>
#include "settings.h"
#include "stringfunc.h"

AppSettings settings;

const char PATH_SETTING_FILENAME_DEPRECATED[] = ".cue-settings";
const char SETTINGS_FILENAME_DEPRECATED[] = "cue.conf";
const char SETTINGS_FILE[] = ".cue.conf";

AppSettings constructAppSettings(KeyValuePair *pairs, int count)
{
    AppSettings settings;
    memset(&settings, 0, sizeof(settings));
    strncpy(settings.coverEnabled, "1", sizeof(settings.coverEnabled));
    strncpy(settings.coverBlocks, "1", sizeof(settings.coverEnabled));
    strncpy(settings.equalizerEnabled, "1", sizeof(settings.equalizerEnabled));
    strncpy(settings.equalizerBlocks, "1", sizeof(settings.equalizerBlocks));

    if (pairs == NULL)
    {
        return settings;
    }

    for (int i = 0; i < count; i++)
    {
        KeyValuePair *pair = &pairs[i];

        if (strcmp(stringToLower(pair->key), "path") == 0)
        {
            snprintf(settings.path, sizeof(settings.path), "%s", pair->value);            
        }
        else if (strcmp(stringToLower(pair->key), "coverenabled") == 0)
        {
            snprintf(settings.coverEnabled, sizeof(settings.coverEnabled), "%s", pair->value);
        }
        else if (strcmp(stringToLower(pair->key), "coverblocks") == 0)
        {
            snprintf(settings.coverBlocks, sizeof(settings.coverBlocks), "%s", pair->value);            
        }
        else if (strcmp(stringToLower(pair->key), "equalizerblocks") == 0)
        {
            snprintf(settings.equalizerBlocks, sizeof(settings.equalizerBlocks), "%s", pair->value);            
        }        
        else if (strcmp(stringToLower(pair->key), "equalizerenabled") == 0)
        {
            snprintf(settings.equalizerEnabled, sizeof(settings.equalizerEnabled), "%s", pair->value);
        }
        else if (strcmp(stringToLower(pair->key), "equalizerheight") == 0)
        {            
            snprintf(settings.equalizerHeight, sizeof(settings.equalizerHeight), "%s", pair->value);
        }
    }

    freeKeyValuePairs(pairs, count);

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
    getSettingsDeprecated(path, MAXPATHLEN, PATH_SETTING_FILENAME_DEPRECATED);

    if (path[0] == '\0')
    {
        struct passwd *pw = getpwuid(getuid());
        strcat(path, pw->pw_dir);
        strcat(path, "/Music/");
    }

    if (expandPath(path, expandedPath))
        strcpy(path, expandedPath);

    return 0;
}

void getConfig()
{
    int pair_count;
    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;
    char* filepath = NULL;
    if (!existsFile(SETTINGS_FILE))
    {
        size_t filepath_length = strlen(homedir) + strlen("/") + strlen(SETTINGS_FILENAME_DEPRECATED) + 1;
        filepath = (char*)malloc(filepath_length);
        strcpy(filepath, homedir);
        strcat(filepath, "/");
        strcat(filepath, SETTINGS_FILENAME_DEPRECATED);
    }
    else
    {
        size_t filepath_length = strlen(homedir) + strlen("/") + strlen(SETTINGS_FILE) + 1;
        filepath = (char*)malloc(filepath_length);
        strcpy(filepath, homedir);
        strcat(filepath, "/");
        strcat(filepath, SETTINGS_FILE);
    }

    KeyValuePair *pairs = readKeyValuePairs(filepath, &pair_count);

    free(filepath);
    settings = constructAppSettings(pairs, pair_count);

    coverEnabled = (settings.coverEnabled[0] == '1');
    coverBlocks = (settings.coverBlocks[0] == '1');
    equalizerEnabled = (settings.equalizerEnabled[0] == '1');
    equalizerBlocks = (settings.equalizerBlocks[0] == '1');
    int temp = atoi(settings.equalizerHeight);
    if (temp > 0)
        equalizerHeight = temp;
    getMusicLibraryPath(settings.path);
}

void setConfig()
{
    // Create the file path
    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;

    char *filepath = (char *)malloc(strlen(homedir) + strlen("/") + strlen(SETTINGS_FILE) + 1);
    strcpy(filepath, homedir);
    strcat(filepath, "/");
    strcat(filepath, SETTINGS_FILE);

    FILE *file = fopen(filepath, "w");
    if (file == NULL)
    {
        fprintf(stderr, "Error opening file: %s\n", filepath);
        free(filepath);
        return;
    }

    if (settings.coverEnabled[0] == '\0')
        coverEnabled ? strcpy(settings.coverEnabled, "1") : strcpy(settings.coverEnabled, "0");
    if (settings.coverBlocks[0] == '\0')
        coverBlocks ? strcpy(settings.coverBlocks, "1") : strcpy(settings.coverBlocks, "0");
    if (settings.equalizerEnabled[0] == '\0')
        equalizerEnabled ? strcpy(settings.equalizerEnabled, "1") : strcpy(settings.equalizerEnabled, "0");
    if (settings.equalizerBlocks[0] == '\0')
        equalizerBlocks ? strcpy(settings.equalizerBlocks, "1") : strcpy(settings.equalizerBlocks, "0");
    if (settings.equalizerHeight[0] == '\0')
    {
        sprintf(settings.equalizerHeight, "%d", equalizerHeight);
    }

    // Null-terminate the character arrays
    settings.path[MAXPATHLEN - 1] = '\0';
    settings.coverEnabled[1] = '\0';
    settings.coverBlocks[1] = '\0';
    settings.equalizerEnabled[1] = '\0';
    settings.equalizerBlocks[1] = '\0';    
    settings.equalizerHeight[5] = '\0';

    // Write the settings to the file
    fprintf(file, "path=%s\n", settings.path);
    fprintf(file, "coverEnabled=%s\n", settings.coverEnabled);
    fprintf(file, "coverBlocks=%s\n", settings.coverBlocks);
    fprintf(file, "equalizerEnabled=%s\n", settings.equalizerEnabled);
    fprintf(file, "equalizerBlocks=%s\n", settings.equalizerBlocks);
    fprintf(file, "equalizerHeight=%s\n", settings.equalizerHeight);

    fclose(file);
    free(filepath);
}