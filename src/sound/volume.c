#include "volume.h"

#include "playback.h"

#include <miniaudio.h>

static int sound_volume = 100;

int get_current_volume(void) {
        return sound_volume;
}

void set_volume(int volume) {
        if (volume > 100) {
                volume = 100;
        } else if (volume < 0) {
                volume = 0;
        }

        sound_volume = volume;

        ma_device_set_master_volume(get_device(), (float)volume / 100);
}

int adjust_volume_percent(int volume_change) {
        sound_volume += volume_change;

        set_volume(sound_volume);

        return 0;
}
