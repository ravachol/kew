#ifndef VOLUME_H
#define VOLUME_H

/**
 * @brief Retrieves the current volume level.
 *
 * This function returns the current sound volume as an integer value. The volume
 * is represented as a percentage (0 to 100).
 *
 * @return The current volume level as a percentage (0 to 100).
 */
int get_current_volume(void);


/**
 * @brief Adjusts the volume by a specified percentage change.
 *
 * This function modifies the current volume by the given change in percentage.
 * Positive values will increase the volume, while negative values will decrease it.
 * It automatically clamps the volume within the valid range of 0 to 100.
 *
 * @param volume_change The percentage change in volume (positive to increase, negative to decrease).
 *
 * @return 0 on success.
 */
int adjust_volume_percent(int volume_change);


/**
 * @brief Sets the volume to a specific level.
 *
 * This function sets the sound volume to the given level, which is clamped within
 * the range of 0 to 100. It also updates the audio device's master volume based on
 * the specified volume.
 *
 * @param volume The desired volume level, expressed as a percentage (0 to 100).
 */
void set_volume(int volume);

#endif
