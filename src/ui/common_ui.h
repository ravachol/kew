/**
 * @file common_ui.h
 * @brief Shared UI utilities and layout helpers.
 *
 * Contains reusable functions for drawing text, handling resizing,
 * and rendering shared UI components across multiple screens.
 */

#ifndef COMMON_UI_H
#define COMMON_UI_H

#include "common/appstate.h"

#include <stdbool.h>

void set_RGB(PixelData p);
void set_album_color(PixelData color);
void increment_update_counter(void);
void inverse_text(void);
void apply_color(ColorMode mode, ColorValue theme_color, PixelData album_color);
void process_name_scroll(const char *name, char *output, int max_width, bool is_same_name_as_last_time);
void reset_name_scroll(void);
void reset_color(void);
void process_name(const char *name, char *output, int max_width, bool strip_unneeded_chars, bool strip_suffix);
void transfer_settings_to_ui(void);
void enable_mouse(UISettings *ui);
int get_update_counter(void);
bool get_is_long_name(void);
enum EventType get_mouse_action(int num);
PixelData increase_luminosity(PixelData pixel, int amount);
PixelData decrease_luminosity_pct(PixelData base, float pct);
PixelData get_gradient_color(PixelData base_color, int row, int max_list_size, int start_gradient, float min_pct);

#endif
