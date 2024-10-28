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

AppSettings settings;

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
#ifdef __APPLE__        
        strncpy(settings.visualizerEnabled, "0", sizeof(settings.visualizerEnabled)); // visualizer looks wonky in default terminal 
        strncpy(settings.useProfileColors, "1", sizeof(settings.useProfileColors));   // colors from album look wrong in default terminal
#else
        strncpy(settings.visualizerEnabled, "1", sizeof(settings.visualizerEnabled));
        strncpy(settings.useProfileColors, "0", sizeof(settings.useProfileColors));
#endif
        strncpy(settings.hideLogo, "0", sizeof(settings.hideLogo));
        strncpy(settings.hideHelp, "0", sizeof(settings.hideHelp));
        strncpy(settings.cacheLibrary, "-1", sizeof(settings.cacheLibrary));

        strncpy(settings.tabNext, "\t", sizeof(settings.tabNext));
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
        strncpy(settings.showPlaylistAlt, "Z", sizeof(settings.showPlaylistAlt));
        strncpy(settings.hardShowKeys, "[17~", sizeof(settings.hardShowKeys));
        strncpy(settings.hardShowKeysAlt, "[17~", sizeof(settings.hardShowKeysAlt));
        strncpy(settings.showKeysAlt, "B", sizeof(settings.showKeysAlt));
        strncpy(settings.hardShowTrack, "OS", sizeof(settings.hardShowTrack));
        strncpy(settings.hardShowTrackAlt, "[[D", sizeof(settings.hardShowTrackAlt));
        strncpy(settings.showTrackAlt, "C", sizeof(settings.showTrackAlt));
        strncpy(settings.hardEndOfPlaylist, "G", sizeof(settings.hardEndOfPlaylist));
        strncpy(settings.hardShowLibrary, "OR", sizeof(settings.hardShowLibrary));
        strncpy(settings.hardShowLibraryAlt, "[[C", sizeof(settings.hardShowLibraryAlt));
        strncpy(settings.showLibraryAlt, "X", sizeof(settings.showLibraryAlt));
        strncpy(settings.hardShowSearch, "[15~", sizeof(settings.hardShowSearch));
        strncpy(settings.hardShowSearchAlt, "[[E", sizeof(settings.hardShowSearchAlt));
        strncpy(settings.showSearchAlt, "V", sizeof(settings.showSearchAlt));
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

                char *lowercaseKey = stringToLower(pair->key); 

                if (strcmp(lowercaseKey, "path") == 0)
                {
                        snprintf(settings.path, sizeof(settings.path), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "coverenabled") == 0)
                {
                        snprintf(settings.coverEnabled, sizeof(settings.coverEnabled), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "coveransi") == 0)
                {
                        snprintf(settings.coverAnsi, sizeof(settings.coverAnsi), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "visualizerenabled") == 0)
                {
                        snprintf(settings.visualizerEnabled, sizeof(settings.visualizerEnabled), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "useprofilecolors") == 0)
                {
                        snprintf(settings.useProfileColors, sizeof(settings.useProfileColors), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "visualizerheight") == 0)
                {
                        snprintf(settings.visualizerHeight, sizeof(settings.visualizerHeight), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "volumeup") == 0)
                {
                        snprintf(settings.volumeUp, sizeof(settings.volumeUp), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "volumeupalt") == 0)
                {
                        snprintf(settings.volumeUpAlt, sizeof(settings.volumeUpAlt), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "volumedown") == 0)
                {
                        snprintf(settings.volumeDown, sizeof(settings.volumeDown), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "previoustrackalt") == 0)
                {
                        snprintf(settings.previousTrackAlt, sizeof(settings.previousTrackAlt), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "nexttrackalt") == 0)
                {
                        snprintf(settings.nextTrackAlt, sizeof(settings.nextTrackAlt), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "scrollupalt") == 0)
                {
                        snprintf(settings.scrollUpAlt, sizeof(settings.scrollUpAlt), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "scrolldownalt") == 0)
                {
                        snprintf(settings.scrollDownAlt, sizeof(settings.scrollDownAlt), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "switchnumberedsong") == 0)
                {
                        snprintf(settings.switchNumberedSong, sizeof(settings.switchNumberedSong), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "togglepause") == 0)
                {
                        snprintf(settings.togglePause, sizeof(settings.togglePause), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "togglecolorsderivedfrom") == 0)
                {
                        snprintf(settings.toggleColorsDerivedFrom, sizeof(settings.toggleColorsDerivedFrom), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "togglevisualizer") == 0)
                {
                        snprintf(settings.toggleVisualizer, sizeof(settings.toggleVisualizer), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "toggleascii") == 0)
                {
                        snprintf(settings.toggleAscii, sizeof(settings.toggleAscii), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "togglerepeat") == 0)
                {
                        snprintf(settings.toggleRepeat, sizeof(settings.toggleRepeat), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "toggleshuffle") == 0)
                {
                        snprintf(settings.toggleShuffle, sizeof(settings.toggleShuffle), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "seekbackward") == 0)
                {
                        snprintf(settings.seekBackward, sizeof(settings.seekBackward), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "seekforward") == 0)
                {
                        snprintf(settings.seekForward, sizeof(settings.seekForward), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "saveplaylist") == 0)
                {
                        snprintf(settings.savePlaylist, sizeof(settings.savePlaylist), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "addtomainplaylist") == 0)
                {
                        snprintf(settings.quit, sizeof(settings.quit), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "lastvolume") == 0)
                {
                        snprintf(settings.lastVolume, sizeof(settings.lastVolume), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "allownotifications") == 0)
                {
                        snprintf(settings.allowNotifications, sizeof(settings.allowNotifications), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "color") == 0)
                {
                        snprintf(settings.color, sizeof(settings.color), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "artistcolor") == 0)
                {
                        snprintf(settings.artistColor, sizeof(settings.artistColor), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "enqueuedcolor") == 0)
                {
                        snprintf(settings.enqueuedColor, sizeof(settings.enqueuedColor), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "titlecolor") == 0)
                {
                        snprintf(settings.titleColor, sizeof(settings.titleColor), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "hidelogo") == 0)
                {
                        snprintf(settings.hideLogo, sizeof(settings.hideLogo), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "hidehelp") == 0)
                {
                        snprintf(settings.hideHelp, sizeof(settings.hideHelp), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "cachelibrary") == 0)
                {
                        snprintf(settings.cacheLibrary, sizeof(settings.cacheLibrary), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "quit") == 0)
                {
                        snprintf(settings.quit, sizeof(settings.quit), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "updatelibrary") == 0)
                {
                        snprintf(settings.updateLibrary, sizeof(settings.updateLibrary), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "showplaylistalt") == 0)
                {
                        if (strcmp(pair->value, "") != 0) // Don't set these to nothing
                                snprintf(settings.showPlaylistAlt, sizeof(settings.showPlaylistAlt), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "showlibraryalt") == 0)
                {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings.showLibraryAlt, sizeof(settings.showLibraryAlt), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "showtrackalt") == 0)
                {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings.showTrackAlt, sizeof(settings.showTrackAlt), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "showsearchalt") == 0)
                {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings.showSearchAlt, sizeof(settings.showSearchAlt), "%s", pair->value);
                }
                else if (strcmp(lowercaseKey, "showkeysalt") == 0)
                {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings.showKeysAlt, sizeof(settings.showKeysAlt), "%s", pair->value);
                }

                free(lowercaseKey);                
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
#ifdef __APPLE__
        *lastTimeAppRan = (file_stat.st_mtime > 0) ? file_stat.st_mtime : file_stat.st_mtimespec.tv_sec;
#else
        *lastTimeAppRan = (file_stat.st_mtime > 0) ? file_stat.st_mtime : file_stat.st_mtim.tv_sec;
#endif
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

const char *getDefaultMusicFolder(void)
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
                {
                        c_strcpy(path, sizeof(expandedPath), expandedPath);
                }
        }

        return 0;
}

void mapSettingsToKeys(AppSettings *settings, EventMapping *mappings)
{
        mappings[0] = (EventMapping){settings->scrollUpAlt, EVENT_SCROLLPREV};
        mappings[1] = (EventMapping){settings->scrollDownAlt, EVENT_SCROLLNEXT};
        mappings[2] = (EventMapping){settings->nextTrackAlt, EVENT_NEXT};
        mappings[3] = (EventMapping){settings->previousTrackAlt, EVENT_PREV};
        mappings[4] = (EventMapping){settings->volumeUp, EVENT_VOLUME_UP};
        mappings[5] = (EventMapping){settings->volumeUpAlt, EVENT_VOLUME_UP};
        mappings[6] = (EventMapping){settings->volumeDown, EVENT_VOLUME_DOWN};
        mappings[7] = (EventMapping){settings->togglePause, EVENT_PLAY_PAUSE};
        mappings[8] = (EventMapping){settings->quit, EVENT_QUIT};
        mappings[9] = (EventMapping){settings->hardQuit, EVENT_QUIT};
        mappings[10] = (EventMapping){settings->toggleShuffle, EVENT_SHUFFLE};
        mappings[11] = (EventMapping){settings->toggleVisualizer, EVENT_TOGGLEVISUALIZER};
        mappings[12] = (EventMapping){settings->toggleAscii, EVENT_TOGGLEBLOCKS};
        mappings[13] = (EventMapping){settings->switchNumberedSong, EVENT_GOTOSONG};
        mappings[14] = (EventMapping){settings->seekBackward, EVENT_SEEKBACK};
        mappings[15] = (EventMapping){settings->seekForward, EVENT_SEEKFORWARD};
        mappings[16] = (EventMapping){settings->toggleRepeat, EVENT_TOGGLEREPEAT};
        mappings[17] = (EventMapping){settings->savePlaylist, EVENT_EXPORTPLAYLIST};
        mappings[18] = (EventMapping){settings->toggleColorsDerivedFrom, EVENT_TOGGLE_PROFILE_COLORS};
        mappings[19] = (EventMapping){settings->addToMainPlaylist, EVENT_ADDTOMAINPLAYLIST};
        mappings[20] = (EventMapping){settings->updateLibrary, EVENT_UPDATELIBRARY};
        mappings[21] = (EventMapping){settings->hardPlayPause, EVENT_PLAY_PAUSE};
        mappings[22] = (EventMapping){settings->hardPrev, EVENT_PREV};
        mappings[23] = (EventMapping){settings->hardNext, EVENT_NEXT};
        mappings[24] = (EventMapping){settings->hardSwitchNumberedSong, EVENT_GOTOSONG};
        mappings[25] = (EventMapping){settings->hardScrollUp, EVENT_SCROLLPREV};
        mappings[26] = (EventMapping){settings->hardScrollDown, EVENT_SCROLLNEXT};
        mappings[27] = (EventMapping){settings->hardShowPlaylist, EVENT_SHOWPLAYLIST};
        mappings[28] = (EventMapping){settings->hardShowPlaylistAlt, EVENT_SHOWPLAYLIST};
        mappings[29] = (EventMapping){settings->showPlaylistAlt, EVENT_SHOWPLAYLIST};
        mappings[30] = (EventMapping){settings->hardShowKeys, EVENT_SHOWKEYBINDINGS};
        mappings[31] = (EventMapping){settings->hardShowKeysAlt, EVENT_SHOWKEYBINDINGS};
        mappings[32] = (EventMapping){settings->showKeysAlt, EVENT_SHOWKEYBINDINGS};
        mappings[33] = (EventMapping){settings->hardEndOfPlaylist, EVENT_GOTOENDOFPLAYLIST};
        mappings[34] = (EventMapping){settings->hardShowTrack, EVENT_SHOWTRACK};
        mappings[35] = (EventMapping){settings->hardShowTrackAlt, EVENT_SHOWTRACK};
        mappings[36] = (EventMapping){settings->showTrackAlt, EVENT_SHOWTRACK};
        mappings[37] = (EventMapping){settings->hardShowLibrary, EVENT_SHOWLIBRARY};
        mappings[38] = (EventMapping){settings->hardShowLibraryAlt, EVENT_SHOWLIBRARY};
        mappings[39] = (EventMapping){settings->showLibraryAlt, EVENT_SHOWLIBRARY};
        mappings[40] = (EventMapping){settings->hardShowSearch, EVENT_SHOWSEARCH};
        mappings[41] = (EventMapping){settings->hardShowSearchAlt, EVENT_SHOWSEARCH};
        mappings[42] = (EventMapping){settings->showSearchAlt, EVENT_SHOWSEARCH};
        mappings[43] = (EventMapping){settings->hardNextPage, EVENT_NEXTPAGE};
        mappings[44] = (EventMapping){settings->hardPrevPage, EVENT_PREVPAGE};
        mappings[45] = (EventMapping){settings->hardRemove, EVENT_REMOVE};
        mappings[46] = (EventMapping){settings->hardRemove2, EVENT_REMOVE};
        mappings[47] = (EventMapping){settings->tabNext, EVENT_TABNEXT};
}

void getConfig(AppSettings *settings, UISettings *ui)
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
        if (filepath == NULL)
        {
                perror("malloc");
                exit(EXIT_FAILURE);
        }
        c_strcpy(filepath, filepath_length, configdir);
        strcat(filepath, "/");
        strcat(filepath, SETTINGS_FILE);

        KeyValuePair *pairs = readKeyValuePairs(filepath, &pair_count, &lastTimeAppRan);

        free(filepath);
        *settings = constructAppSettings(pairs, pair_count);

        ui->allowNotifications = (settings->allowNotifications[0] == '1');
        ui->coverEnabled = (settings->coverEnabled[0] == '1');
        ui->coverAnsi = (settings->coverAnsi[0] == '1');
        ui->visualizerEnabled = (settings->visualizerEnabled[0] == '1');
        ui->useProfileColors = (settings->useProfileColors[0] == '1');
        ui->hideLogo = (settings->hideLogo[0] == '1');
        ui->hideHelp = (settings->hideHelp[0] == '1');

        int temp = atoi(settings->color);
        if (temp >= 0)
                ui->mainColor = temp;
        temp = atoi(settings->artistColor);
        if (temp >= 0)
                ui->artistColor = temp;

        temp = atoi(settings->enqueuedColor);
        if (temp >= 0)
                ui->enqueuedColor = temp;

        temp = atoi(settings->titleColor);
        if (temp >= 0)
                ui->titleColor = temp;

        int temp2 = atoi(settings->visualizerHeight);
        if (temp2 > 0)
                ui->visualizerHeight = temp2;

        int temp3 = atoi(settings->lastVolume);
        if (temp3 >= 0)
                setVolume(temp3);

        int temp4 = atoi(settings->cacheLibrary);
        if (temp4 >= 0)
                ui->cacheLibrary = temp4;

        getMusicLibraryPath(settings->path);
        free(configdir);
}

void setConfig(AppSettings *settings, UISettings *ui)
{
        // Create the file path
        char *configdir = getConfigPath();

        size_t filepath_length = strlen(configdir) + strlen("/") + strlen(SETTINGS_FILE) + 1;
        char *filepath = (char *)malloc(filepath_length);
        if (filepath == NULL)
        {
                perror("malloc");
                exit(EXIT_FAILURE);
        }
        c_strcpy(filepath, filepath_length, configdir);
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
                ui->allowNotifications ? c_strcpy(settings->allowNotifications, sizeof(settings->allowNotifications), "1") : c_strcpy(settings->allowNotifications, sizeof(settings->allowNotifications), "0");
        if (settings->coverEnabled[0] == '\0')
                ui->coverEnabled ? c_strcpy(settings->coverEnabled, sizeof(settings->coverEnabled), "1") : c_strcpy(settings->coverEnabled, sizeof(settings->coverEnabled), "0");
        if (settings->coverAnsi[0] == '\0')
                ui->coverAnsi ? c_strcpy(settings->coverAnsi, sizeof(settings->coverAnsi), "1") : c_strcpy(settings->coverAnsi, sizeof(settings->coverAnsi), "0");
        if (settings->visualizerEnabled[0] == '\0')
                ui->visualizerEnabled ? c_strcpy(settings->visualizerEnabled, sizeof(settings->visualizerEnabled), "1") : c_strcpy(settings->visualizerEnabled, sizeof(settings->visualizerEnabled), "0");
        if (settings->useProfileColors[0] == '\0')
                ui->useProfileColors ? c_strcpy(settings->useProfileColors, sizeof(settings->useProfileColors), "1") : c_strcpy(settings->useProfileColors, sizeof(settings->useProfileColors), "0");
        if (settings->visualizerHeight[0] == '\0')
        {
                snprintf(settings->visualizerHeight, sizeof(settings->visualizerHeight), "%d", ui->visualizerHeight);
        }
        if (settings->hideLogo[0] == '\0')
                ui->hideLogo ? c_strcpy(settings->hideLogo, sizeof(settings->hideLogo), "1") : c_strcpy(settings->hideLogo, sizeof(settings->hideLogo), "0");
        if (settings->hideHelp[0] == '\0')
                ui->hideHelp ? c_strcpy(settings->hideHelp, sizeof(settings->hideHelp), "1") : c_strcpy(settings->hideHelp, sizeof(settings->hideHelp), "0");

        snprintf(settings->cacheLibrary, sizeof(settings->cacheLibrary), "%d", ui->cacheLibrary);

        int currentVolume = getCurrentVolume();
        currentVolume = (currentVolume <= 0) ? 10 : currentVolume;

        snprintf(settings->lastVolume, sizeof(settings->lastVolume), "%d", currentVolume);

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
        fprintf(file, "# Logo color:\n");
        fprintf(file, "color=%s\n", settings->color);
        fprintf(file, "# Header color in library view:\n");
        fprintf(file, "artistColor=%s\n", settings->artistColor);
        fprintf(file, "# Now playing song text in library view:\n");
        fprintf(file, "titleColor=%s\n", settings->titleColor);
        fprintf(file, "# Color of enqueued songs in library view:\n");
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
        fprintf(file, "\n# The different main views, normally F2-F6:\n");
        fprintf(file, "showPlaylistAlt=%s\n", settings->showPlaylistAlt);
        fprintf(file, "showLibraryAlt=%s\n", settings->showLibraryAlt);
        fprintf(file, "showTrackAlt=%s\n", settings->showTrackAlt);
        fprintf(file, "showSearchAlt=%s\n", settings->showSearchAlt);
        fprintf(file, "showKeysAlt=%s\n\n", settings->showKeysAlt);

        fprintf(file, "quit=%s\n\n", settings->quit);
        fprintf(file, "# For special keys use terminal codes: OS, for F4 for instance. This can depend on the terminal.\n");
        fprintf(file, "# You can find out the codes for the keys by using tools like showkey.\n");
        fprintf(file, "# For special keys, see the key value after the bracket \"[\" after typing \"showkey -a\" in the terminal and then pressing a key you want info about.\n");

        fclose(file);
        free(filepath);
        free(configdir);
}
