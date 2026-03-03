/**
 * @file volume.h
 * @brief Get and set volume.
 *
 */

#ifndef VOLUME_H
#define VOLUME_H

/**
 * @brief Retrieves the current volume level.
 *
 * This function returns the current sound volume as an integer value.
 *
 * @return The current volume level as a float (0.0f to 1.0f).
 */
float get_current_volume(void);

/**
 * @brief Sets the volume to a specific level.
 *
 * This function sets the sound volume to the given level, which is clamped within
 * the range of 0 to 100. It also updates the audio device's master volume based on
 * the specified volume.
 *
 * @param volume The desired volume level, expressed as a float (0.0f to 1.0f).
 */
void set_current_volume(float volume);

#endif
