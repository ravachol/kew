#include <stdio.h>

int adjustVolumePercent(int volumeChange)
{
    char command_str[1000];

    if (volumeChange > 0)
      snprintf(command_str, 1000, "amixer -D pulse sset Master %d%%+", volumeChange);      
    else
      snprintf(command_str, 1000, "amixer -D pulse sset Master %d%%-", -volumeChange);
            
    // Open the command for reading. 
    FILE *fp = popen(command_str, "r");
    if (fp == NULL) {
        return -1;
    }

    // Close the command stream. 
    pclose(fp);
    return 0;
}