#include <stdbool.h>
#include <string.h>
#include "settings.h"
#include "stringfunc.h"

AppSettings settings;

const char SETTINGS_FILE[] = ".cue.conf";

AppSettings constructAppSettings(KeyValuePair *pairs, int count)
{
    AppSettings settings;
    memset(&settings, 0, sizeof(settings));
    strncpy(settings.coverEnabled, "1", sizeof(settings.coverEnabled));
    strncpy(settings.coverAnsi, "0", sizeof(settings.coverAnsi));
    strncpy(settings.visualizerEnabled, "0", sizeof(settings.visualizerEnabled));

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
        else if (strcmp(stringToLower(pair->key), "coveransi") == 0)
        {
            snprintf(settings.coverAnsi, sizeof(settings.coverAnsi), "%s", pair->value);
        }
        else if (strcmp(stringToLower(pair->key), "visualizerenabled") == 0)
        {
            snprintf(settings.visualizerEnabled, sizeof(settings.visualizerEnabled), "%s", pair->value);
        }    
        else if (strcmp(stringToLower(pair->key), "visualizerheight") == 0)
        {
            snprintf(settings.visualizerHeight, sizeof(settings.visualizerHeight), "%s", pair->value);
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

int getMusicLibraryPath(char *path)
{
    char expandedPath[MAXPATHLEN];

    if (path[0] != '\0' && path[0] != '\r')
    {
        if (expandPath(path, expandedPath) >= 0)
            strcpy(path, expandedPath);
        return 0;
    }

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
    char *filepath = NULL;

    size_t filepath_length = strlen(homedir) + strlen("/") + strlen(SETTINGS_FILE) + 1;
    filepath = (char *)malloc(filepath_length);
    strcpy(filepath, homedir);
    strcat(filepath, "/");
    strcat(filepath, SETTINGS_FILE);

    KeyValuePair *pairs = readKeyValuePairs(filepath, &pair_count);

    free(filepath);
    settings = constructAppSettings(pairs, pair_count);

    coverEnabled = (settings.coverEnabled[0] == '1');
    coverAnsi = (settings.coverAnsi[0] == '1');
    visualizerEnabled = (settings.visualizerEnabled[0] == '1');
    int temp = atoi(settings.visualizerHeight);
    if (temp > 0)
        visualizerHeight = temp;
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
    if (settings.coverAnsi[0] == '\0')
        coverAnsi ? strcpy(settings.coverAnsi, "1") : strcpy(settings.coverAnsi, "0");
    if (settings.visualizerEnabled[0] == '\0')
        visualizerEnabled ? strcpy(settings.visualizerEnabled, "1") : strcpy(settings.visualizerEnabled, "0");    
    if (settings.visualizerHeight[0] == '\0')
    {
        sprintf(settings.visualizerHeight, "%d", visualizerHeight);
    }     

    // Null-terminate the character arrays
    settings.path[MAXPATHLEN - 1] = '\0';
    settings.coverEnabled[1] = '\0';
    settings.coverAnsi[1] = '\0';
    settings.visualizerEnabled[1] = '\0';
    settings.visualizerHeight[5] = '\0';

    // Write the settings to the file
    fprintf(file, "path=%s\n", settings.path);
    //fprintf(file, "useThemeColors=%s\n", settings.useThemeColors);    
    fprintf(file, "coverEnabled=%s\n", settings.coverEnabled);
    fprintf(file, "coverAnsi=%s\n", settings.coverAnsi);
    fprintf(file, "visualizerEnabled=%s\n", settings.visualizerEnabled);
    fprintf(file, "visualizerHeight=%s\n", settings.visualizerHeight);    

    fclose(file);
    free(filepath);
}