#include <stdbool.h>
#include <string.h>
#include "settings.h"
#include "stringfunc.h"
/*

settings.c

 Functions related to the config file.
 
*/

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

AppSettings settings;

const char OLD_SETTINGS_FILE[] = ".cue.conf";
const char NEW_SETTINGS_FILE[] = "kewrc";

AppSettings constructAppSettings(KeyValuePair *pairs, int count)
{
        AppSettings settings;
        memset(&settings, 0, sizeof(settings));
        strncpy(settings.coverEnabled, "1", sizeof(settings.coverEnabled));
        strncpy(settings.coverAnsi, "0", sizeof(settings.coverAnsi));
        strncpy(settings.visualizerEnabled, "1", sizeof(settings.visualizerEnabled));
        strncpy(settings.useProfileColors, "1", sizeof(settings.useProfileColors));

        strncpy(settings.volumeUp, "+", sizeof(settings.volumeUp));
        strncpy(settings.volumeDown, "-", sizeof(settings.volumeDown));
        strncpy(settings.previousTrackAlt, "h", sizeof(settings.previousTrackAlt));
        strncpy(settings.nextTrackAlt, "l", sizeof(settings.nextTrackAlt));
        strncpy(settings.scrollUpAlt, "k", sizeof(settings.scrollUpAlt));
        strncpy(settings.scrollDownAlt, "j", sizeof(settings.scrollDownAlt));
        strncpy(settings.toggleColorsDerivedFrom, "i", sizeof(settings.toggleColorsDerivedFrom));
        strncpy(settings.toggleVisualizer, "v", sizeof(settings.toggleVisualizer));
        strncpy(settings.toggleCovers, "c", sizeof(settings.toggleCovers));
        strncpy(settings.toggleAscii, "b", sizeof(settings.toggleAscii));
        strncpy(settings.toggleRepeat, "r", sizeof(settings.toggleRepeat));
        strncpy(settings.toggleShuffle, "s", sizeof(settings.toggleShuffle));
        strncpy(settings.togglePause, "p", sizeof(settings.togglePause));
        strncpy(settings.seekBackward, "a", sizeof(settings.seekBackward));
        strncpy(settings.seekForward, "d", sizeof(settings.seekForward));
        strncpy(settings.savePlaylist, "x", sizeof(settings.savePlaylist));
        strncpy(settings.addToMainPlaylist, ".", sizeof(settings.addToMainPlaylist));
        strncpy(settings.hardPlayPause, " ", sizeof(settings.hardPlayPause));
        strncpy(settings.hardSwitchNumberedSong, "\n", sizeof(settings.hardSwitchNumberedSong));
        strncpy(settings.hardPrev, "[D", sizeof(settings.hardPrev));
        strncpy(settings.hardNext, "[C", sizeof(settings.hardNext));
        strncpy(settings.hardScrollUp, "[A", sizeof(settings.hardScrollUp));
        strncpy(settings.hardScrollDown, "[B", sizeof(settings.hardScrollDown));
        strncpy(settings.hardShowInfo, "OQ", sizeof(settings.hardShowInfo));
        strncpy(settings.hardShowInfoAlt, "[[B", sizeof(settings.hardShowInfoAlt));
        strncpy(settings.hardShowKeys, "OR", sizeof(settings.hardShowKeys));
        strncpy(settings.hardShowKeysAlt, "[[C", sizeof(settings.hardShowKeysAlt));
        strncpy(settings.hardEndOfPlaylist, "G", sizeof(settings.hardEndOfPlaylist));
        strncpy(settings.quit, "q", sizeof(settings.quit));

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
                else if (strcmp(stringToLower(pair->key), "useprofilecolors") == 0)
                {
                        snprintf(settings.useProfileColors, sizeof(settings.useProfileColors), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "visualizerheight") == 0)
                {
                        snprintf(settings.visualizerHeight, sizeof(settings.visualizerHeight), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "volumeup") == 0)
                {
                        snprintf(settings.volumeUp, sizeof(settings.volumeUp), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "volumedown") == 0)
                {
                        snprintf(settings.volumeDown, sizeof(settings.volumeDown), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "previoustrackalt") == 0)
                {
                        snprintf(settings.previousTrackAlt, sizeof(settings.previousTrackAlt), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "nexttrackalt") == 0)
                {
                        snprintf(settings.nextTrackAlt, sizeof(settings.nextTrackAlt), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "scrollupalt") == 0)
                {
                        snprintf(settings.scrollUpAlt, sizeof(settings.scrollUpAlt), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "scrolldownalt") == 0)
                {
                        snprintf(settings.scrollDownAlt, sizeof(settings.scrollDownAlt), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "switchnumberedsong") == 0)
                {
                        snprintf(settings.switchNumberedSong, sizeof(settings.switchNumberedSong), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "togglepause") == 0)
                {
                        snprintf(settings.togglePause, sizeof(settings.togglePause), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "togglecolorsderivedfrom") == 0)
                {
                        snprintf(settings.toggleColorsDerivedFrom, sizeof(settings.toggleColorsDerivedFrom), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "togglevisualizer") == 0)
                {
                        snprintf(settings.toggleVisualizer, sizeof(settings.toggleVisualizer), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "togglecovers") == 0)
                {
                        snprintf(settings.toggleCovers, sizeof(settings.toggleCovers), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "toggleascii") == 0)
                {
                        snprintf(settings.toggleAscii, sizeof(settings.toggleAscii), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "togglerepeat") == 0)
                {
                        snprintf(settings.toggleRepeat, sizeof(settings.toggleRepeat), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "toggleshuffle") == 0)
                {
                        snprintf(settings.toggleShuffle, sizeof(settings.toggleShuffle), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "seekbackward") == 0)
                {
                        snprintf(settings.seekBackward, sizeof(settings.seekBackward), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "seekforward") == 0)
                {
                        snprintf(settings.seekForward, sizeof(settings.seekForward), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "saveplaylist") == 0)
                {
                        snprintf(settings.savePlaylist, sizeof(settings.savePlaylist), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "addtomainplaylist") == 0)
                {
                        snprintf(settings.quit, sizeof(settings.quit), "%s", pair->value);
                }                
                else if (strcmp(stringToLower(pair->key), "quit") == 0)
                {
                        snprintf(settings.quit, sizeof(settings.quit), "%s", pair->value);
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
                                char *home = (char *)malloc(PATH_MAX);
                                strcpy(home, pw->pw_dir);
                                return home;
                        }
                }
        }
        return xdgHome;
}

const char *getConfigPath()
{
        const char *xdgConfig = getenv("XDG_CONFIG_HOME");
        if (xdgConfig == NULL)
        {
                // If XDG_CONFIG_HOME is not set, fall back to the default location
                const char *home = getHomePath();
                if (home != NULL)
                {
                        // Construct the default configuration path
                        char *configPath = (char *)malloc(PATH_MAX);
                        if (configPath != NULL)
                        {
                                snprintf(configPath, PATH_MAX, "%s/.config", home);
                                if (isDirectory(configPath))
                                {
                                        xdgConfig = configPath;
                                }
                                else
                                {
                                        free(configPath); // Free the allocated memory
                                        return home;
                                }
                        }
                }
                else
                {
                        struct passwd *pw = getpwuid(getuid());
                        if (pw != NULL)
                        {
                                return pw->pw_dir;
                        }
                }
        }
        return xdgConfig;
}
const char *getDefaultMusicFolder()
{
        const char *home = getHomePath();
        if (home != NULL)
        {
                static char musicPath[PATH_MAX];
                snprintf(musicPath, sizeof(musicPath), "%s/Music", home);
                return musicPath;
        }
        else
        {
                return NULL; // Return NULL if XDG home is not found.
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

        if (path[0] == '\0')
        {
                const char *musicFolder = getDefaultMusicFolder();
                if (musicFolder != NULL)
                        strcpy(path, musicFolder);
        }

        if (expandPath(path, expandedPath))
                strcpy(path, expandedPath);

        return 0;
}

void getConfigOld()
{
        int pair_count;
        struct passwd *pw = getpwuid(getuid());
        const char *homedir = pw->pw_dir;
        char *filepath = NULL;

        size_t filepath_length = strlen(homedir) + strlen("/") + strlen(OLD_SETTINGS_FILE) + 1;
        filepath = (char *)malloc(filepath_length);
        strcpy(filepath, homedir);
        strcat(filepath, "/");
        strcat(filepath, OLD_SETTINGS_FILE);

        KeyValuePair *pairs = readKeyValuePairs(filepath, &pair_count);

        free(filepath);
        settings = constructAppSettings(pairs, pair_count);

        coverEnabled = (settings.coverEnabled[0] == '1');
        coverAnsi = (settings.coverAnsi[0] == '1');
        visualizerEnabled = (settings.visualizerEnabled[0] == '1');
        useProfileColors = (settings.useProfileColors[0] == '1');
        int temp = atoi(settings.visualizerHeight);
        if (temp > 0)
                visualizerHeight = temp;
        getMusicLibraryPath(settings.path);
}

void getConfig()
{
        int pair_count;
        const char *configdir = getConfigPath();
        char *filepath = NULL;

        size_t filepath_length = strlen(configdir) + strlen("/") + strlen(NEW_SETTINGS_FILE) + 1;
        filepath = (char *)malloc(filepath_length);
        strcpy(filepath, configdir);
        strcat(filepath, "/");
        strcat(filepath, NEW_SETTINGS_FILE);

        if (existsFile(filepath) < 0)
        {
                free(filepath);
                getConfigOld(); // Get config from the old location
                return;
        }

        KeyValuePair *pairs = readKeyValuePairs(filepath, &pair_count);

        free(filepath);
        settings = constructAppSettings(pairs, pair_count);

        coverEnabled = (settings.coverEnabled[0] == '1');
        coverAnsi = (settings.coverAnsi[0] == '1');
        visualizerEnabled = (settings.visualizerEnabled[0] == '1');
        useProfileColors = (settings.useProfileColors[0] == '1');
        int temp = atoi(settings.visualizerHeight);
        if (temp > 0)
                visualizerHeight = temp;
        getMusicLibraryPath(settings.path);
}

void setConfig()
{
        // Create the file path
        const char *configdir = getConfigPath();

        char *filepath = (char *)malloc(strlen(configdir) + strlen("/") + strlen(NEW_SETTINGS_FILE) + 1);
        strcpy(filepath, configdir);
        strcat(filepath, "/");
        strcat(filepath, NEW_SETTINGS_FILE);

        FILE *file = fopen(filepath, "w");
        if (file == NULL)
        {
                fprintf(stderr, "Error opening file: %s\n", filepath);
                free(filepath);
                return;
        }

        if (settings.coverEnabled[0] == '\0')
                coverEnabled ? c_strcpy(settings.coverEnabled, sizeof(settings.coverEnabled), "1") : c_strcpy(settings.coverEnabled, sizeof(settings.coverEnabled), "0");
        if (settings.coverAnsi[0] == '\0')
                coverAnsi ? c_strcpy(settings.coverAnsi, sizeof(settings.coverAnsi), "1") : c_strcpy(settings.coverAnsi, sizeof(settings.coverAnsi), "0");
        if (settings.visualizerEnabled[0] == '\0')
                visualizerEnabled ? c_strcpy(settings.visualizerEnabled, sizeof(settings.visualizerEnabled), "1") : c_strcpy(settings.visualizerEnabled, sizeof(settings.visualizerEnabled), "0");
        if (settings.useProfileColors[0] == '\0')
                useProfileColors ? c_strcpy(settings.useProfileColors, sizeof(settings.useProfileColors), "1") : c_strcpy(settings.useProfileColors, sizeof(settings.useProfileColors), "0");
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
        settings.useProfileColors[1] = '\0';

        // Write the settings to the file
        fprintf(file, "path=%s\n", settings.path);
        // fprintf(file, "useThemeColors=%s\n", settings.useThemeColors);
        fprintf(file, "coverEnabled=%s\n", settings.coverEnabled);
        fprintf(file, "coverAnsi=%s\n", settings.coverAnsi);
        fprintf(file, "visualizerEnabled=%s\n", settings.visualizerEnabled);
        fprintf(file, "visualizerHeight=%s\n", settings.visualizerHeight);
        fprintf(file, "useProfileColors=%s\n", settings.useProfileColors);
        fprintf(file, "volumeUp=%s\n", settings.volumeUp);
        fprintf(file, "volumeDown=%s\n", settings.volumeDown);
        fprintf(file, "previousTrackAlt=%s\n", settings.previousTrackAlt);
        fprintf(file, "nextTrackAlt=%s\n", settings.nextTrackAlt);
        fprintf(file, "scrollUpAlt=%s\n", settings.scrollUpAlt);
        fprintf(file, "scrollDownAlt=%s\n", settings.scrollDownAlt);
        fprintf(file, "switchNumberedSong=%s\n", settings.switchNumberedSong);
        fprintf(file, "togglePause=%s\n", settings.togglePause);
        fprintf(file, "toggleColorsDerivedFrom=%s\n", settings.toggleColorsDerivedFrom);
        fprintf(file, "toggleVisualizer=%s\n", settings.toggleVisualizer);
        fprintf(file, "toggleCovers=%s\n", settings.toggleCovers);
        fprintf(file, "toggleAscii=%s\n", settings.toggleAscii);
        fprintf(file, "toggleRepeat=%s\n", settings.toggleRepeat);
        fprintf(file, "toggleShuffle=%s\n", settings.toggleShuffle);
        fprintf(file, "seekBackward=%s\n", settings.seekBackward);
        fprintf(file, "seekForward=%s\n", settings.seekForward);
        fprintf(file, "savePlaylist=%s\n", settings.savePlaylist);
        fprintf(file, "addToMainPlaylist=%s\n", settings.addToMainPlaylist);
        fprintf(file, "quit=%s\n", settings.quit);
        fprintf(file, "# For special keys use terminal codes OS, for F4 for instance. This can depend on the terminal.");

        fclose(file);
        free(filepath);
}
