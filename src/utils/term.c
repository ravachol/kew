/**
 * @file term.c
 * @brief Terminal manipulation utilities.
 *
 * Handles terminal capabilities (color, cursor movement, screen clearing),
 * and provides a lightweight abstraction for TUI rendering.
 */

#include "term.h"

#include <fcntl.h>
#include <poll.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

static const int MAX_TERMINAL_ROWS = 9999;
static struct termios orig_termios;
static int termios_saved = 0;

void set_terminal_color(int color)
{
        /*
        - 0: Black
        - 1: Red
        - 2: Green
        - 3: Yellow
        - 4: Blue
        - 5: Magenta
        - 6: Cyan
        - 7: White
        - 8: Bright Black (Gray)
        - 9: Bright Red
        - 10: Bright Green
        - 11: Bright Yellow
        - 12: Bright Blue
        - 13: Bright Magenta
        - 14: Bright Cyan
        - 15: Bright White
        */

        if (color < -1 || color > 15)
                color = 7; // default to white

        if (color == -1) {
                // Default foreground
                printf("\033[39m");
        } else if (color < 8) {
                // Normal colors (30–37)
                printf("\033[0;3%dm", color);
        } else {
                // Bright colors (90–97)
                printf("\033[0;9%dm", color - 8);
        }
}

void set_text_color_RGB(int r, int g, int b)
{
        if (r < 0 || r > 255)
                r = 255;
        if (g < 0 || g > 255)
                g = 255;
        if (b < 0 || b > 255)
                b = 255;

        printf("\033[0;38;2;%03u;%03u;%03um", (unsigned int)r, (unsigned int)g,
               (unsigned int)b);
}

void get_term_size(int *width, int *height)
{
        struct winsize w;

        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1 ||
            w.ws_row == 0 || w.ws_col == 0) {
                // Fallback for non-interactive environments (like Homebrew tests)
                *height = 24; // default terminal height
                *width = 80;  // default terminal width
                return;
        }

        *height = (int)w.ws_row;
        *width = (int)w.ws_col;
}

void set_nonblocking_mode(void)
{
        struct termios ttystate;
        tcgetattr(STDIN_FILENO, &orig_termios); // save original

        termios_saved = 1;

        ttystate = orig_termios;
        ttystate.c_lflag &= ~(ICANON | ECHO); // non-canonical, no echo
        // ISIG left intact to handle Ctrl-C etc.
        ttystate.c_cc[VMIN] = 0; // return immediately
        ttystate.c_cc[VTIME] = 0;

        tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
        tcflush(STDIN_FILENO, TCIFLUSH); // flush any leftover input
}

void restore_terminal_mode(void)
{
        if (termios_saved) {
                tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
                tcflush(STDIN_FILENO, TCIFLUSH);
        }
}

void save_cursor_position(void)
{
        printf("\033[s");
}

void restore_cursor_position(void)
{
        printf("\033[u");
}

void set_default_text_color(void)
{
        printf("\033[0m");
}

void hide_cursor(void)
{
        printf("\033[?25l");
}

void show_cursor(void)
{
        printf("\033[?25h");
        fflush(stdout);
}

void clear_rest_of_screen(void)
{
        printf("\033[J");
}

void clear_line(void)
{
        printf("\033[2K");
}

void clear_rest_of_line(void)
{
        printf("\033[K");
}

void clear_screen(void)
{
        printf("\033[3J");              // Clear scrollback buffer
        printf("\033[2J\033[3J\033[H"); // Move cursor to top-left and clear
                                        // screen and scrollback buffer
}

void goto_first_line_first_row(void)
{
        printf("\033[H");
}

void enable_scrolling(void)
{
        printf("\033[?7h");
}

void disable_terminal_line_input(void)
{
        setvbuf(stdout, NULL, _IOFBF, BUFSIZ);
}

void set_raw_input_mode(void)
{
        struct termios term;
        tcgetattr(STDIN_FILENO, &term);
        term.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &term);
}

void enable_input_buffering()
{
        struct termios term;
        tcgetattr(STDIN_FILENO, &term);
        term.c_lflag |= ICANON | ECHO;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &term);
}

void cursor_jump(int num_rows)
{
        if (num_rows < 0 || num_rows > MAX_TERMINAL_ROWS)
                return;

        printf("\033[%dA", num_rows);
        printf("\033[0m");
}

void cursor_jump_down(int num_rows)
{
        if (num_rows < 0 || num_rows > MAX_TERMINAL_ROWS)
                return;

        printf("\033[%dB", num_rows);
}

int read_input_sequence(char *seq, size_t seq_size)
{
        if (seq == NULL ||
            seq_size < 2) // Buffer needs at least 1 byte + null terminator
                return 0;

        char c;
        ssize_t bytes_read = read(STDIN_FILENO, &c, 1);
        if (bytes_read <= 0)
                return 0;

        // ASCII character (single byte, no continuation bytes)
        if ((c & 0x80) == 0) {
                if (seq_size <
                    2) // Make sure there's space for the null terminator
                        return 0;
                seq[0] = c;
                seq[1] = '\0';
                return 1;
        }

        // Determine the length of the UTF-8 sequence and validate the first
        // byte
        int additional_bytes;
        if ((c & 0xE0) == 0xC0)
                additional_bytes = 1; // 2-byte sequence
        else if ((c & 0xF0) == 0xE0)
                additional_bytes = 2; // 3-byte sequence
        else if ((c & 0xF8) == 0xF0)
                additional_bytes = 3; // 4-byte sequence
        else
                return 0; // Invalid UTF-8 start byte

        if ((size_t)(additional_bytes + 1) >= seq_size)
                return 0;

        seq[0] = c;

        // Read the continuation bytes
        bytes_read = read(STDIN_FILENO, &seq[1], additional_bytes);
        if (bytes_read != additional_bytes)
                return 0;

        // Validate continuation bytes (0x80 <= byte <= 0xBF)
        for (int i = 1; i <= additional_bytes; ++i) {
                if ((seq[i] & 0xC0) != 0x80)
                        return 0; // Invalid continuation byte
        }

        // Null terminate the string
        seq[additional_bytes + 1] = '\0';

        return additional_bytes +
               1; // Return the total length including the null terminator
}

int is_input_available(void)
{
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0;

        int ret = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);

        if (ret < 0) {
                return 0;
        }
        int result = (ret > 0) && (FD_ISSET(STDIN_FILENO, &fds));
        return result;
}

int get_indentation(int text_width)
{
        int term_w, term_h;
        get_term_size(&term_w, &term_h);

        if (text_width <= 0 || term_w <= 0) {
                return 0;
        }

        if (text_width >= term_w) {
                text_width = term_w;
        }

        int available_space = term_w - text_width;
        int indent = (available_space / 2) + 1;

        return indent;
}

void enter_alternate_screen_buffer(void)
{
        // Enter alternate screen buffer
        printf("\033[?1049h");
}

void exit_alternate_screen_buffer(void)
{
        // Exit alternate screen buffer
        printf("\033[?1049l");
}

void enable_terminal_mouse_buttons(void)
{
        // Enable program to accept mouse input as codes
        printf("\033[?1002h\033[?1006h");
}

void disable_terminal_mouse_buttons(void)
{
        // Disable ALL mouse modes that might have been enabled
        // Some terminals auto-enable 1004 when SGR mouse mode is on.
        // VTE (Guake) will not clear it unless explicitly disabled.
        printf("\033[?1000l");  // X10 mouse
        printf("\033[?1001l");  // Highlight tracking
        printf("\033[?1002l");  // Button tracking
        printf("\033[?1003l");  // Any-motion tracking
        printf("\033[?1004l");  // Focus events (VTE uses this!)
        printf("\033[?1006l");  // SGR mouse mode
        printf("\033[?1015l");  // urxvt mouse mode (rare but harmless)
        fflush(stdout);
}

void set_terminal_window_title(char *title)
{
        // Only change window title, no icon
        printf("\033]2;%s\007", title);
}

void save_terminal_window_title(void)
{
        // Save terminal window title on the stack
        printf("\033[22;0t");
}

void restore_terminal_window_title(void)
{
        // Restore terminal window title from the stack
        printf("\033[23;0t");
}
