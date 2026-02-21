/**
 * @file term.h
 * @brief Terminal manipulation utilities.
 *
 * Handles terminal capabilities (color, cursor movement, screen clearing),
 * and provides a lightweight abstraction for TUI rendering.
 */

#ifndef TERM_H

#define TERM_H

#include <stdio.h>

#ifndef __USE_POSIX
#define __USE_POSIX
#endif

#ifdef __GNU__
#define _BSD_SOURCE
#endif

/**
 * @brief Sets the terminal text color based on an indexed color.
 *
 * This function sets the foreground text color of the terminal. It supports both standard
 * and bright colors (0–15). It can reset to the default foreground color with `-1`.
 *
 * @param color The color index to set. Values range from 0 to 15.
 *        The color values correspond to:
 *        - 0: Black
 *        - 1: Red
 *        - 2: Green
 *        - 3: Yellow
 *        - 4: Blue
 *        - 5: Magenta
 *        - 6: Cyan
 *        - 7: White
 *        - 8: Bright Black (Gray)
 *        - 9: Bright Red
 *        - 10: Bright Green
 *        - 11: Bright Yellow
 *        - 12: Bright Blue
 *        - 13: Bright Magenta
 *        - 14: Bright Cyan
 *        - 15: Bright White
 */
void set_terminal_color(int color);


/**
 * @brief Sets the terminal text color using RGB values.
 *
 * This function sets the terminal text color using RGB values, allowing for a wider range
 * of colors than the indexed approach.
 *
 * @param r Red component of the RGB color (0-255).
 * @param g Green component of the RGB color (0-255).
 * @param b Blue component of the RGB color (0-255).
 */
void set_text_color_RGB(int r, int g, int b);


/**
 * @brief Sets the terminal size to the current terminal dimensions.
 *
 * This function queries the terminal for its current size and updates the internal size variables.
 * If the terminal size is unavailable (in non-interactive environments), it sets defaults.
 */
void set_term_size(void);


/**
 * @brief Retrieves the current terminal size.
 *
 * This function retrieves the current width and height of the terminal and stores them in
 * the provided pointers.
 *
 * @param width Pointer to an integer where the terminal width will be stored.
 * @param height Pointer to an integer where the terminal height will be stored.
 */
void get_term_size(int *width, int *height);

/**
 * @brief Calculates the indentation for text to be centered on the terminal.
 *
 * This function calculates the number of spaces to indent a line of text
 * such that it is centered within the terminal's width. If the text is wider
 * than the terminal, it is truncated to fit.
 *
 * @param text_width The width of the text to be centered.
 *
 * @return The number of spaces to indent the text, or 0 if the text width
 *         is invalid or the terminal width is unavailable.
 */
int get_indentation(int text_width);


/**
 * @brief Sets the terminal to non-blocking mode.
 *
 * This function changes the terminal mode to non-blocking, which allows for immediate
 * reading of input without waiting for the user to press Enter.
 */
void set_nonblocking_mode(void);


/**
 * @brief Restores the terminal to its original mode.
 *
 * This function restores the terminal's settings to their original state, reversing
 * any changes made by `set_nonblocking_mode()`.
 */
void restore_terminal_mode(void);

/**
 * @brief Reads an input sequence from standard input and stores it in a buffer.
 *
 * This function reads a sequence of bytes from the standard input, handling
 * both single-byte ASCII characters and multi-byte UTF-8 sequences. It stores
 * the sequence in the provided buffer and ensures the buffer can hold the
 * characters along with a null terminator.
 *
 * @param seq A pointer to a buffer where the input sequence will be stored.
 *            The buffer should be large enough to store the sequence and a null terminator.
 * @param seq_size The size of the buffer. It must be at least 2 to store the character and the null terminator.
 *
 * @return The length of the input sequence (including the null terminator), or 0 if an error occurs or the input is invalid.
 *         Returns 1 for a single-byte ASCII character, or more for multi-byte UTF-8 characters.
 */
int read_input_sequence(char *seq, size_t seq_size);


/**
 * @brief Checks if input is available on the standard input (stdin).
 *
 * This function uses the `select()` system call to check if there is any input
 * available for reading from the standard input. It returns a boolean value
 * indicating whether data is ready to be read.
 *
 * @return 1 if input is available on stdin, 0 if no input is available or
 *         if there was an error while checking the input availability.
 */
int is_input_available(void);


/**
 * @brief Saves the current cursor position.
 *
 * This function saves the cursor's current position, allowing it to be restored later
 * using `restore_cursor_position()`.
 */
void save_cursor_position(void);


/**
 * @brief Restores the cursor to its previously saved position.
 *
 * This function restores the cursor to the position saved by `save_cursor_position()`.
 */
void restore_cursor_position(void);


/**
 * @brief Sets the terminal text color to the default color.
 *
 * This function resets the terminal's text color to the default color, typically black or white.
 */
void set_default_text_color(void);


/**
 * @brief Hides the terminal cursor.
 *
 * This function hides the cursor in the terminal, often used during animations or to prevent
 * distracting visuals.
 */
void hide_cursor(void);


/**
 * @brief Shows the terminal cursor.
 *
 * This function makes the terminal cursor visible again after being hidden using `hide_cursor()`.
 */
void show_cursor(void);


/**
 * @brief Clears the rest of the screen from the current cursor position.
 *
 * This function clears the screen from the current cursor position to the bottom of the terminal.
 */
void clear_rest_of_screen(void);


/**
 * @brief Clears the current line in the terminal.
 *
 * This function clears the entire current line, including the text and any additional formatting.
 */
void clear_line(void);


/**
 * @brief Clears the rest of the line from the current cursor position.
 *
 * This function clears the part of the current line from the cursor to the end of the line.
 */
void clear_rest_of_line(void);


/**
 * @brief Moves the cursor to the first row and first column.
 *
 * This function moves the terminal cursor to the very top-left corner of the terminal.
 */
void goto_first_line_first_row(void);


/**
 * @brief Initializes terminal resizing settings.
 *
 * This function sets up terminal resize handling, likely enabling proper re-layout during
 * terminal resizing.
 */
void init_resize(void);


/**
 * @brief Disables terminal line input buffering.
 *
 * This function disables line input buffering, meaning input will be processed immediately without
 * waiting for the Enter key.
 */
void disable_terminal_line_input(void);


/**
 * @brief Sets the terminal to raw input mode.
 *
 * This function puts the terminal into raw input mode, where input is immediately processed
 * without line buffering, echoing, or other terminal settings.
 */
void set_raw_input_mode(void);


/**
 * @brief Enables input buffering in the terminal.
 *
 * This function restores the terminal's default line input mode, where input is buffered until
 * the user presses Enter.
 */
void enable_input_buffering(void);


/**
 * @brief Enables terminal scrolling.
 *
 * This function sends an ANSI escape sequence to the terminal to enable
 * scrolling, allowing content to scroll past the terminal's viewport instead
 * of being clipped. It is typically used when the terminal has been configured
 * to prevent scrolling.
 *
 * @note This is useful when the terminal has been manually configured to
 *       disable scrolling, and you want to restore the default scrolling
 *       behavior.
 */
 void enable_scrolling(void);

/**
 * @brief Jumps the cursor up by a specified number of rows.
 *
 * This function moves the cursor up by `num_rows` rows in the terminal. The number of rows must
 * be within valid bounds.
 *
 * @param num_rows The number of rows to move the cursor up.
 */
void cursor_jump(int num_rows);


/**
 * @brief Jumps the cursor down by a specified number of rows.
 *
 * This function moves the cursor down by `num_rows` rows in the terminal. The number of rows must
 * be within valid bounds.
 *
 * @param num_rows The number of rows to move the cursor down.
 */
void cursor_jump_down(int num_rows);


/**
 * @brief Clears the entire screen and scrollback buffer.
 *
 * This function clears the screen and the scrollback buffer, effectively resetting the terminal's
 * display.
 */
void clear_screen(void);


/**
 * @brief Enters the alternate screen buffer.
 *
 * This function switches the terminal to an alternate screen buffer, typically used for full-screen
 * applications like text editors.
 */
void enter_alternate_screen_buffer(void);


/**
 * @brief Exits the alternate screen buffer.
 *
 * This function returns the terminal to the main screen buffer after being switched to the alternate
 * buffer with `enter_alternate_screen_buffer()`.
 */
void exit_alternate_screen_buffer(void);


/**
 * @brief Enables terminal mouse input for button tracking.
 *
 * This function enables mouse tracking for the terminal, allowing the program to detect mouse events
 * (button presses, mouse movements, etc.).
 */
void enable_terminal_mouse_buttons(void);


/**
 * @brief Disables terminal mouse input.
 *
 * This function disables all mouse input tracking in the terminal.
 */
void disable_terminal_mouse_buttons(void);


/**
 * @brief Sets the terminal window title.
 *
 * This function sets the terminal window title to the given string, allowing users to set custom
 * titles for their terminal sessions.
 *
 * @param title The new window title.
 */
void set_terminal_window_title(char *title);


/**
 * @brief Saves the current terminal window title.
 *
 * This function saves the current terminal window title, allowing it to be restored later.
 */
void save_terminal_window_title(void);


/**
 * @brief Restores the terminal window title to the saved title.
 *
 * This function restores the terminal window title to the one saved previously using `save_terminal_window_title()`.
 */
void restore_terminal_window_title(void);

#endif
