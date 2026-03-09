/**
 * @file volume.c
 * @brief Get and set volume.
 *
 */
#include "volume.h"

#include "playback.h"

#include <miniaudio.h>

float sound_volume = 1.0f;

float get_current_volume(void)
{
        return sound_volume;
}

void set_current_volume(float volume)
{
        if (volume > 1.0f) {
                volume = 1.0f;
        } else if (volume < 0.0f) {
                volume = 0.0f;
        }

        sound_volume = volume;

        ma_device_set_master_volume(get_device(), volume);
}
