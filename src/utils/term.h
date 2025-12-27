/**
 * @file term.h
 * @brief Terminal manipulation utilities.
 *
 * Handles terminal capabilities (color, cursor movement, screen clearing),
 * and provides a lightweight abstraction for TUI rendering.
 */

#ifndef TERM_H

#include <stdio.h>

#define TERM_H
#ifndef __USE_POSIX
#define __USE_POSIX
#endif

#ifdef __GNU__
#define _BSD_SOURCE
#endif

int get_indentation(int text_width);
int is_input_available(void);
int read_input_sequence(char *seq, size_t seq_size);
void set_terminal_color(int color);
void set_text_color_RGB(int r, int g, int b);
void update_term_size();
void get_term_size(int *width, int *height);
void set_nonblocking_mode(void);
void restore_terminal_mode(void);
void set_default_text_color(void);
void save_cursor_position(void);
void restore_cursor_position(void);
void hide_cursor(void);
void show_cursor(void);
void clear_rest_of_screen(void);
void enable_scrolling(void);
void clear_line(void);
void clear_rest_of_line(void);
void goto_first_line_first_row(void);
void init_resize(void);
void disable_terminal_line_input(void);
void set_raw_input_mode(void);
void enable_input_buffering(void);
void cursor_jump(int num_rows);
void cursor_jump_down(int num_rows);
void clear_screen(void);
void enter_alternate_screen_buffer(void);
void exit_alternate_screen_buffer(void);
void enable_terminal_mouse_buttons(void);
void disable_terminal_mouse_buttons(void);
void set_terminal_window_title(char *title);
void save_terminal_window_title(void);
void restore_terminal_window_title(void);
void saveOriginalTerminalMode(void);
void restore_terminal_mode(void);

#endif
