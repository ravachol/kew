#include "volume.h"
/*

volume.c

 Functions related to volume control.
 
*/
int getCurrentVolume()
{
        FILE *fp;
        char command_str[1000];
        char buf[100];
        int currentVolume = -1;

        // Build the command string
        snprintf(command_str, sizeof(command_str),
                 "pactl get-sink-volume @DEFAULT_SINK@ | awk 'NR==1{print $5}'");

        // Execute the command and read the output
        fp = popen(command_str, "r");
        if (fp != NULL)
        {
                if (fgets(buf, sizeof(buf), fp) != NULL)
                {
                        sscanf(buf, "%d", &currentVolume);
                }
                pclose(fp);
        }

        return currentVolume;
}

void setVolume(int volume)
{
        char command_str[1000];
        FILE *fp;

        // Limit new volume to a maximum of 100%
        if (volume > 100)
        {
                volume = 100;
        }
        else if (volume < 0)
        {
                volume = 0;
        }

        snprintf(command_str, 1000, "pactl set-sink-volume @DEFAULT_SINK@ %d%%", volume);

        fp = popen(command_str, "r");
        if (fp == NULL)
        {
                return;
        }
        pclose(fp);
}

int adjustVolumePercent(int volumeChange)
{
        char command_str[1000];
        FILE *fp;
        int currentVolume = getCurrentVolume();

        int newVolume = currentVolume + volumeChange;

        // Limit new volume to a maximum of 100%
        if (newVolume > 100)
        {
                newVolume = 100;
        }
        else if (newVolume < 0)
        {
                newVolume = 0;
        }

        snprintf(command_str, 1000, "pactl set-sink-volume @DEFAULT_SINK@ %d%%", newVolume);

        fp = popen(command_str, "r");
        if (fp == NULL)
        {
                return -1;
        }
        pclose(fp);

        return 0;
}