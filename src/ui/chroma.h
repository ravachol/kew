/**
 * @file chroma.h
 * @brief To handle running Chroma in a thread and printing its output
 *
 * Contains functions to start, stop and print a chroma frame.
 */

#ifndef CHROMA_H
#define CHROMA_H

#include <stdbool.h>

/**
 * Starts Chroma (externally provided visualisations).
 *
 * Launches background rendering.
 *
 * @param height  Height of terminal window in characters
 */
void chroma_start(int height);

/**
 * Stops Chroma (externally provided visualisations).
 *
 * Stops a chroma_thread and frees resources.
 *
 */
void chroma_stop(void);

/**
 * Prints a Chroma frame to terminal window.
 *
 * @param row  The row where this frame should be printed
 * @param col  The col where this frame should be printed
 * @param centered  whether the frame should be centered (col ignored)
 */
void chroma_print_frame(int row, int col, bool centered);

/**
 * Changes the Chroma (externally provided visualisations) preset to the next.
 *
 * Stops visualizations if we hit MAX_PRESET
 *
 * @param height  Height of terminal window in characters
 */
void chroma_set_next_preset(int height);

/**
 * Returns true if Chroma (externally provided visualisations) is installed.
 *
 * @return true if Chroma is available, false otherwise
 */
bool chroma_is_installed(void);

/**
 * Returns true if Chroma (externally provided visualisations) is started.
 *
 * @return true if Chroma is started, false otherwise
 */
bool chroma_is_started(void);

/**
 * Returns the current Chroma preset number.
 *
 */
int chroma_get_current_preset(void);

/**
 * Sets the current Chroma preset number.
 *
 * @param preset Preset index to activate
 */
void chroma_set_current_preset(int preset);

#endif
