#include <pwd.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// saves the path to your music folder
int saveSettings(char *path, const char* settingsFile);

// reads the settings file, which contains the path to your music folder
int getsettings(char *path, int len, const char* settingsFile);