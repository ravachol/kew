
/**
 * @file visuals.h
 * @brief Audio visualization rendering.
 *
 * Implements a spectrum visualizer that react
 * to playback data.
 */

void init_visuals(void);
void free_visuals(void);
void draw_spectrum_visualizer(int row, int col, int height, int width);
