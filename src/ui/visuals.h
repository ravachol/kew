
/**
 * @file visuals.h
 * @brief Audio visualization rendering.
 *
 * Implements a spectrum visualizer that react
 * to playback data.
 */

/**
 * @brief Frees allocated memory for the visualizer resources.
 *
 * This function deallocates memory used by the FFT input and output buffers.
 * It ensures that any dynamically allocated resources for the visualizer are properly freed.
 */
void free_visuals(void);

/**
 * @brief Draws the spectrum visualizer on the screen.
 *
 * This function draws a spectrum visualizer in the given area of the screen.
 * It calculates the frequency magnitudes and displays the visual representation of the audio data
 * in the specified position and size.
 *
 * @param row The row position (top-left corner) where the visualizer should be drawn.
 * @param col The column position (top-left corner) where the visualizer should be drawn.
 * @param height The height of the visualizer in rows.
 * @param width The width of the visualizer in columns.
 */
void draw_spectrum_visualizer(int row, int col, int height, int width);
