#include "settings.h"

/*

settings.c

 Functions related to the config file.

*/

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

const char SETTINGS_FILE[] = "kewrc";

time_t lastTimeAppRan;

void freeKeyValuePairs(KeyValuePair *pairs, int count)
{
        for (int i = 0; i < count; i++)
        {
                free(pairs[i].key);
                free(pairs[i].value);
        }

        free(pairs);
}

AppSettings constructAppSettings(KeyValuePair *pairs, int count)
{
        AppSettings settings;
        memset(&settings, 0, sizeof(settings));
        strncpy(settings.coverEnabled, "1", sizeof(settings.coverEnabled));
        strncpy(settings.allowNotifications, "1", sizeof(settings.allowNotifications));
        strncpy(settings.coverAnsi, "0", sizeof(settings.coverAnsi));
        strncpy(settings.visualizerEnabled, "1", sizeof(settings.visualizerEnabled));
        strncpy(settings.useProfileColors, "0", sizeof(settings.useProfileColors));
        strncpy(settings.hideLogo, "0", sizeof(settings.hideLogo));
        strncpy(settings.hideHelp, "0", sizeof(settings.hideHelp));
        strncpy(settings.cacheLibrary, "-1", sizeof(settings.cacheLibrary));

        strncpy(settings.volumeUp, "+", sizeof(settings.volumeUp));
        strncpy(settings.volumeUpAlt, "=", sizeof(settings.volumeUpAlt));
        strncpy(settings.volumeDown, "-", sizeof(settings.volumeDown));
        strncpy(settings.previousTrackAlt, "h", sizeof(settings.previousTrackAlt));
        strncpy(settings.nextTrackAlt, "l", sizeof(settings.nextTrackAlt));
        strncpy(settings.scrollUpAlt, "k", sizeof(settings.scrollUpAlt));
        strncpy(settings.scrollDownAlt, "j", sizeof(settings.scrollDownAlt));
        strncpy(settings.toggleColorsDerivedFrom, "i", sizeof(settings.toggleColorsDerivedFrom));
        strncpy(settings.toggleVisualizer, "v", sizeof(settings.toggleVisualizer));
        strncpy(settings.toggleAscii, "b", sizeof(settings.toggleAscii));
        strncpy(settings.toggleRepeat, "r", sizeof(settings.toggleRepeat));
        strncpy(settings.toggleShuffle, "s", sizeof(settings.toggleShuffle));
        strncpy(settings.togglePause, "p", sizeof(settings.togglePause));
        strncpy(settings.seekBackward, "a", sizeof(settings.seekBackward));
        strncpy(settings.seekForward, "d", sizeof(settings.seekForward));
        strncpy(settings.savePlaylist, "x", sizeof(settings.savePlaylist));
        strncpy(settings.updateLibrary, "u", sizeof(settings.updateLibrary));
        strncpy(settings.addToMainPlaylist, ".", sizeof(settings.addToMainPlaylist));
        strncpy(settings.hardPlayPause, " ", sizeof(settings.hardPlayPause));
        strncpy(settings.hardSwitchNumberedSong, "\n", sizeof(settings.hardSwitchNumberedSong));
        strncpy(settings.hardPrev, "[D", sizeof(settings.hardPrev));
        strncpy(settings.hardNext, "[C", sizeof(settings.hardNext));
        strncpy(settings.hardScrollUp, "[A", sizeof(settings.hardScrollUp));
        strncpy(settings.hardScrollDown, "[B", sizeof(settings.hardScrollDown));
        strncpy(settings.hardShowPlaylist, "OQ", sizeof(settings.hardShowPlaylist));
        strncpy(settings.hardShowPlaylistAlt, "[[B", sizeof(settings.hardShowPlaylistAlt));
        strncpy(settings.showPlaylistAlt, "", sizeof(settings.showPlaylistAlt));
        strncpy(settings.hardShowKeys, "[17~", sizeof(settings.hardShowKeys));
        strncpy(settings.hardShowKeysAlt, "[17~", sizeof(settings.hardShowKeysAlt));
        strncpy(settings.showKeysAlt, "", sizeof(settings.showKeysAlt));
        strncpy(settings.hardShowTrack, "OS", sizeof(settings.hardShowTrack));
        strncpy(settings.hardShowTrackAlt, "[[D", sizeof(settings.hardShowTrackAlt));
        strncpy(settings.showTrackAlt, "", sizeof(settings.showTrackAlt));
        strncpy(settings.hardEndOfPlaylist, "G", sizeof(settings.hardEndOfPlaylist));
        strncpy(settings.hardShowLibrary, "OR", sizeof(settings.hardShowLibrary));
        strncpy(settings.hardShowLibraryAlt, "[[C", sizeof(settings.hardShowLibraryAlt));
        strncpy(settings.showLibraryAlt, "", sizeof(settings.showLibraryAlt));
        strncpy(settings.hardShowSearch, "[15~", sizeof(settings.hardShowSearch));
        strncpy(settings.hardShowSearchAlt, "[[E", sizeof(settings.hardShowSearchAlt));
        strncpy(settings.showSearchAlt, "", sizeof(settings.showSearchAlt));
        strncpy(settings.hardNextPage, "[6~", sizeof(settings.hardNextPage));
        strncpy(settings.hardPrevPage, "[5~", sizeof(settings.hardPrevPage));
        strncpy(settings.hardRemove, "[3~", sizeof(settings.hardRemove));
        strncpy(settings.hardRemove2, "[P", sizeof(settings.hardRemove2));
        strncpy(settings.lastVolume, "100", sizeof(settings.lastVolume));
        strncpy(settings.color, "6", sizeof(settings.color));
        strncpy(settings.artistColor, "6", sizeof(settings.artistColor));
        strncpy(settings.titleColor, "6", sizeof(settings.titleColor));
        strncpy(settings.enqueuedColor, "7", sizeof(settings.enqueuedColor));
        strncpy(settings.quit, "q", sizeof(settings.quit));
        strncpy(settings.hardQuit, "\x1B", sizeof(settings.hardQuit));

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
                else if (strcmp(stringToLower(pair->key), "volumeupalt") == 0)
                {
                        snprintf(settings.volumeUpAlt, sizeof(settings.volumeUpAlt), "%s", pair->value);
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
                else if (strcmp(stringToLower(pair->key), "lastvolume") == 0)
                {
                        snprintf(settings.lastVolume, sizeof(settings.lastVolume), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "allownotifications") == 0)
                {
                        snprintf(settings.allowNotifications, sizeof(settings.allowNotifications), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "color") == 0)
                {
                        snprintf(settings.color, sizeof(settings.color), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "artistcolor") == 0)
                {
                        snprintf(settings.artistColor, sizeof(settings.artistColor), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "enqueuedcolor") == 0)
                {
                        snprintf(settings.enqueuedColor, sizeof(settings.enqueuedColor), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "titlecolor") == 0)
                {
                        snprintf(settings.titleColor, sizeof(settings.titleColor), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "hidelogo") == 0)
                {
                        snprintf(settings.hideLogo, sizeof(settings.hideLogo), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "hidehelp") == 0)
                {
                        snprintf(settings.hideHelp, sizeof(settings.hideHelp), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "cachelibrary") == 0)
                {
                        snprintf(settings.cacheLibrary, sizeof(settings.cacheLibrary), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "quit") == 0)
                {
                        snprintf(settings.quit, sizeof(settings.quit), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "updatelibrary") == 0)
                {
                        snprintf(settings.updateLibrary, sizeof(settings.updateLibrary), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "showplaylistalt") == 0)
                {
                        snprintf(settings.showPlaylistAlt, sizeof(settings.showPlaylistAlt), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "showlibraryalt") == 0)
                {
                        snprintf(settings.showLibraryAlt, sizeof(settings.showLibraryAlt), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "showtrackalt") == 0)
                {
                        snprintf(settings.showTrackAlt, sizeof(settings.showTrackAlt), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "showsearchalt") == 0)
                {
                        snprintf(settings.showSearchAlt, sizeof(settings.showSearchAlt), "%s", pair->value);
                }
                else if (strcmp(stringToLower(pair->key), "showkeysalt") == 0)
                {
                        snprintf(settings.showKeysAlt, sizeof(settings.showKeysAlt), "%s", pair->value);
                }
        }

        freeKeyValuePairs(pairs, count);

        return settings;
}

KeyValuePair *readKeyValuePairs(const char *file_path, int *count, time_t *lastTimeAppRan)
{
        FILE *file = fopen(file_path, "r");
        if (file == NULL)
        {
                return NULL;
        }

        struct stat file_stat;
        if (stat(file_path, &file_stat) == -1)
        {
                perror("stat");
                return NULL;
        }

        // Save the modification time (mtime) of the file
        *lastTimeAppRan = (file_stat.st_mtime > 0) ? file_stat.st_mtime : file_stat.st_mtim.tv_sec;

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

const char *getDefaultMusicFolder()
{
        const char *home = getHomePath();
        if (home != NULL)
        {
                static char musicPath[MAXPATHLEN];
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
        }

        return 0;
}

void getConfig(AppSettings *settings)
{
        int pair_count;
        char *configdir = getConfigPath();
        char *filepath = NULL;

        // Create the directory if it doesn't exist
        struct stat st = {0};
        if (stat(configdir, &st) == -1)
        {
                if (mkdir(configdir, 0700) != 0)
                {
                        perror("mkdir");
                        exit(EXIT_FAILURE);
                }
        }

        size_t filepath_length = strlen(configdir) + strlen("/") + strlen(SETTINGS_FILE) + 1;
        filepath = (char *)malloc(filepath_length);
        strcpy(filepath, configdir);
        strcat(filepath, "/");
        strcat(filepath, SETTINGS_FILE);

        KeyValuePair *pairs = readKeyValuePairs(filepath, &pair_count, &lastTimeAppRan);

        free(filepath);
        *settings = constructAppSettings(pairs, pair_count);

        allowNotifications = (settings->allowNotifications[0] == '1');
        coverEnabled = (settings->coverEnabled[0] == '1');
        coverAnsi = (settings->coverAnsi[0] == '1');
        visualizerEnabled = (settings->visualizerEnabled[0] == '1');
        useProfileColors = (settings->useProfileColors[0] == '1');
        hideLogo = (settings->hideLogo[0] == '1');
        hideHelp = (settings->hideHelp[0] == '1');

        int temp = atoi(settings->color);
        if (temp >= 0)
                mainColor = temp;
        temp = atoi(settings->artistColor);
        if (temp >= 0)
                artistColor = temp;

        temp = atoi(settings->enqueuedColor);
        if (temp >= 0)
                enqueuedColor = temp;

        temp = atoi(settings->titleColor);
        if (temp >= 0)
                titleColor = temp;

        int temp2 = atoi(settings->visualizerHeight);
        if (temp2 > 0)
                visualizerHeight = temp2;

        int temp3 = atoi(settings->lastVolume);
        if (temp3 >= 0)
                setVolume(temp3);

        int temp4 = atoi(settings->cacheLibrary);
        if (temp4 >= 0)
                cacheLibrary = temp4;

        getMusicLibraryPath(settings->path);
        free(configdir);
}

void setConfig(AppSettings *settings)
{
        // Create the file path
        char *configdir = getConfigPath();

        char *filepath = (char *)malloc(strlen(configdir) + strlen("/") + strlen(SETTINGS_FILE) + 1);
        strcpy(filepath, configdir);
        strcat(filepath, "/");
        strcat(filepath, SETTINGS_FILE);

        FILE *file = fopen(filepath, "w");
        if (file == NULL)
        {
                fprintf(stderr, "Error opening file: %s\n", filepath);
                free(filepath);
                free(configdir);
                return;
        }

        if (settings->allowNotifications[0] == '\0')
                allowNotifications ? c_strcpy(settings->allowNotifications, sizeof(settings->allowNotifications), "1") : c_strcpy(settings->allowNotifications, sizeof(settings->allowNotifications), "0");
        if (settings->coverEnabled[0] == '\0')
                coverEnabled ? c_strcpy(settings->coverEnabled, sizeof(settings->coverEnabled), "1") : c_strcpy(settings->coverEnabled, sizeof(settings->coverEnabled), "0");
        if (settings->coverAnsi[0] == '\0')
                coverAnsi ? c_strcpy(settings->coverAnsi, sizeof(settings->coverAnsi), "1") : c_strcpy(settings->coverAnsi, sizeof(settings->coverAnsi), "0");
        if (settings->visualizerEnabled[0] == '\0')
                visualizerEnabled ? c_strcpy(settings->visualizerEnabled, sizeof(settings->visualizerEnabled), "1") : c_strcpy(settings->visualizerEnabled, sizeof(settings->visualizerEnabled), "0");
        if (settings->useProfileColors[0] == '\0')
                useProfileColors ? c_strcpy(settings->useProfileColors, sizeof(settings->useProfileColors), "1") : c_strcpy(settings->useProfileColors, sizeof(settings->useProfileColors), "0");
        if (settings->visualizerHeight[0] == '\0')
        {
                sprintf(settings->visualizerHeight, "%d", visualizerHeight);
        }
        if (settings->hideLogo[0] == '\0')
                hideLogo ? c_strcpy(settings->hideLogo, sizeof(settings->hideLogo), "1") : c_strcpy(settings->hideLogo, sizeof(settings->hideLogo), "0");
        if (settings->hideHelp[0] == '\0')
                hideHelp ? c_strcpy(settings->hideHelp, sizeof(settings->hideHelp), "1") : c_strcpy(settings->hideHelp, sizeof(settings->hideHelp), "0");

        sprintf(settings->cacheLibrary, "%d", cacheLibrary);

        int currentVolume = getCurrentVolume();
        currentVolume = (currentVolume <= 0) ? 10 : currentVolume;

        sprintf(settings->lastVolume, "%d", currentVolume);

        // Null-terminate the character arrays
        settings->path[MAXPATHLEN - 1] = '\0';
        settings->coverEnabled[1] = '\0';
        settings->coverAnsi[1] = '\0';
        settings->visualizerEnabled[1] = '\0';
        settings->visualizerHeight[5] = '\0';
        settings->lastVolume[5] = '\0';
        settings->useProfileColors[1] = '\0';
        settings->allowNotifications[1] = '\0';
        settings->hideLogo[1] = '\0';
        settings->hideHelp[1] = '\0';
        settings->cacheLibrary[5] = '\0';

        // Write the settings to the file
        fprintf(file, "# Make sure that kew is closed before editing this file in order for changes to take effect.\n\n");

        fprintf(file, "path=%s\n", settings->path);
        // fprintf(file, "useThemeColors=%s\n", settings->useThemeColors);
        fprintf(file, "coverEnabled=%s\n", settings->coverEnabled);
        fprintf(file, "coverAnsi=%s\n", settings->coverAnsi);
        fprintf(file, "visualizerEnabled=%s\n", settings->visualizerEnabled);
        fprintf(file, "visualizerHeight=%s\n", settings->visualizerHeight);
        fprintf(file, "useProfileColors=%s\n", settings->useProfileColors);
        fprintf(file, "allowNotifications=%s\n", settings->allowNotifications);
        fprintf(file, "hideLogo=%s\n", settings->hideLogo);
        fprintf(file, "hideHelp=%s\n", settings->hideHelp);
        fprintf(file, "lastVolume=%s\n", settings->lastVolume);

        fprintf(file, "\n# Cache: Set to 1 to use cache of the music library directory tree for faster startup times.\n");
        fprintf(file, "cacheLibrary=%s\n", settings->cacheLibrary);

        fprintf(file, "\n# Color values are 0=Black, 1=Red, 2=Green, 3=Yellow, 4=Blue, 5=Magenta, 6=Cyan, 7=White\n");
        fprintf(file, "# These mostly affect the library view.\n\n");
        fprintf(file, "# Logo color: \n");
        fprintf(file, "color=%s\n", settings->color);
        fprintf(file, "# Header color in library view: \n");
        fprintf(file, "artistColor=%s\n", settings->artistColor);
        fprintf(file, "# Now playing song text in library view: \n");
        fprintf(file, "titleColor=%s\n", settings->titleColor);
        fprintf(file, "# Color of enqueued songs in library view: \n");
        fprintf(file, "enqueuedColor=%s\n", settings->enqueuedColor);

        fprintf(file, "\n# Key Bindings:\n\n");
        fprintf(file, "volumeUp=%s\n", settings->volumeUp);
        fprintf(file, "volumeUpAlt=%s\n", settings->volumeUpAlt);
        fprintf(file, "volumeDown=%s\n", settings->volumeDown);
        fprintf(file, "previousTrackAlt=%s\n", settings->previousTrackAlt);
        fprintf(file, "nextTrackAlt=%s\n", settings->nextTrackAlt);
        fprintf(file, "scrollUpAlt=%s\n", settings->scrollUpAlt);
        fprintf(file, "scrollDownAlt=%s\n", settings->scrollDownAlt);
        fprintf(file, "switchNumberedSong=%s\n", settings->switchNumberedSong);
        fprintf(file, "togglePause=%s\n", settings->togglePause);
        fprintf(file, "toggleColorsDerivedFrom=%s\n", settings->toggleColorsDerivedFrom);
        fprintf(file, "toggleVisualizer=%s\n", settings->toggleVisualizer);
        fprintf(file, "toggleAscii=%s\n", settings->toggleAscii);
        fprintf(file, "toggleRepeat=%s\n", settings->toggleRepeat);
        fprintf(file, "toggleShuffle=%s\n", settings->toggleShuffle);
        fprintf(file, "seekBackward=%s\n", settings->seekBackward);
        fprintf(file, "seekForward=%s\n", settings->seekForward);
        fprintf(file, "savePlaylist=%s\n", settings->savePlaylist);
        fprintf(file, "addToMainPlaylist=%s\n", settings->addToMainPlaylist);
        fprintf(file, "updateLibrary=%s\n", settings->updateLibrary);
        fprintf(file, "\n# The different main views, normally F2-F6: \n");
        fprintf(file, "showPlaylistAlt=%s\n", settings->showPlaylistAlt);
        fprintf(file, "showLibraryAlt=%s\n", settings->showLibraryAlt);
        fprintf(file, "showTrackAlt=%s\n", settings->showTrackAlt);
        fprintf(file, "showSearchAlt=%s\n", settings->showSearchAlt);
        fprintf(file, "showKeysAlt=%s\n\n", settings->showKeysAlt);

        fprintf(file, "quit=%s\n\n", settings->quit);
        fprintf(file, "# For special keys use terminal codes: OS, for F4 for instance. This can depend on the terminal.\n");
        fprintf(file, "# You can find out the codes for the keys by using tools like showkey.\n");
        fprintf(file, "# For special keys, see the key value after the bracket \"[\" after typing \"showkey -a\" in the terminal and then pressing a key you want info about.\n");
        fprintf(file, "\n\n");

        fclose(file);
        free(filepath);
        free(configdir);
}
