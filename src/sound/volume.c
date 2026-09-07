/**
 * @file volume.c
 * @brief Get and set volume.
 *
 */
#include "volume.h"

#include "playback.h"

#include <math.h>
#include <miniaudio.h>

float sound_volume = 1.0f;

float get_current_volume(void)
{
        return round(sound_volume * 100.0) / 100.0;
}

void set_current_volume(float volume)
{
        if (volume > 1.200f) {
                volume = 1.200f;
        } else if (volume < 0.0f) {
                volume = 0.0f;
        }

        sound_volume = round(volume * 100.0) / 100.0;

        ma_device_set_master_volume(get_device(), volume);
}
