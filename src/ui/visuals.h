
/**
 * @file visuals.h
 * @brief Audio visualization rendering.
 *
 * Implements ASCII or terminal-based visualizers that react
 * to playback data such as amplitude or frequency spectrum.
 */

void initVisuals(void);
void free_visuals(void);
void draw_spectrum_visualizer(int row, int col, int height);
