/**
 * @file visualizations.h
 * @brief To handle running visualizations in a thread and printing its output
 *
 * Contains functions to start, stop and print a Visualizations frame.
 */

#ifndef VISUALIZATIONS_H
#define VISUALIZATIONS_H

#include <stdbool.h>

/**
 * Starts Visualizations (externally provided visualisations).
 *
 * Launches background rendering.
 *
 * @param height  Height of terminal window in characters
 * @param width   Width of terminal window in characters
 */
void visualizations_start(int height, int width);

/**
 * Stops Visualizations (externally provided visualisations).
 *
 * Stops a visualizations_thread and frees resources.
 *
 */
void visualizations_shutdown(void);

/**
 * Prints a Visualizations frame to a buffer
 *
 * @param row  The row where this frame should be printed
 * @param col  The col where this frame should be printed
 * @param height
 * @param width
 * @param centered  whether the frame should be centered (col ignored)
 */
void visualizations_print_frame(int row, int col, int height, int width, bool centered);

/**
 * Changes the Visualizations (externally provided visualisations) preset to the next.
 *
 */
void visualizations_set_next_preset(void);

void visualizations_start_with_preset(int preset);

/**
 * Returns true if visualizer appname is installed.
 *
 * @return true if visualizer is available, false otherwise
 */
bool visualizations_are_installed(char *appname);

/**
 * Returns true if Visualizations (externally provided visualisations) is started.
 *
 * @return true if Visualizations is started, false otherwise
 */
bool visualizations_are_started(void);

/**
 * Returns the current Visualizations preset number.
 *
 */
int visualizations_get_current_preset(void);

/**
 * Sets the current Visualizations preset number.
 *
 * @param preset Preset index to activate
 */
void visualizations_set_current_preset(int preset);

#endif
