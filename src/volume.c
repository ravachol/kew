#include "volume.h"
/*

volume.c

 Functions related to volume control.

*/

int soundVolume = 100;

int getCurrentVolume()
{
        return soundVolume;
}

int getSystemVolume()
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
        if (volume > 100)
        {
                volume = 100;
        }
        else if (volume < 0)
        {
                volume = 0;
        }

        soundVolume = volume;

        ma_device_set_master_volume(getDevice(), (float)volume / 100);
}

int adjustVolumePercent(int volumeChange)
{
        int sysVol = getSystemVolume();

        if (sysVol == 0) 
                return 0;

        int step = 100 / sysVol * 5;

        int relativeVolChange = volumeChange / 5  * step;

        soundVolume += relativeVolChange;

        setVolume(soundVolume);

        return 0;
}