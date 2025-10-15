#include "volume.h"

#include "playback.h"

#include <miniaudio.h>

static int soundVolume = 100;

int getCurrentVolume(void) { return soundVolume; }

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
        soundVolume += volumeChange;

        setVolume(soundVolume);

        return 0;
}
