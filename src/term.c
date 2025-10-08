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

/*

term.c

 This file should contain only simple utility functions related to the terminal.
 They should work independently and be as decoupled from the application as
possible.

*/

static const int MAX_TERMINAL_ROWS = 9999;

void setTerminalColor(int color)
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

        if (color == -1)
        {
                // Default foreground
                printf("\033[39m");
        }
        else if (color < 8)
        {
                // Normal colors (30–37)
                printf("\033[0;3%dm", color);
        }
        else
        {
                // Bright colors (90–97)
                printf("\033[0;9%dm", color - 8);
        }
}

void setTextColorRGB(int r, int g, int b)
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

void getTermSize(int *width, int *height)
{
    struct winsize w;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1 ||
        w.ws_row == 0 || w.ws_col == 0)
    {
        // Fallback for non-interactive environments (like Homebrew tests)
        *height = 24;  // default terminal height
        *width = 80;   // default terminal width
        return;
    }

    *height = (int)w.ws_row;
    *width = (int)w.ws_col;
}

void setNonblockingMode(void)
{
        struct termios ttystate;
        tcgetattr(STDIN_FILENO, &ttystate);
        ttystate.c_lflag &= ~ICANON;
        ttystate.c_cc[VMIN] = 0;
        ttystate.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}

void restoreTerminalMode(void)
{
        struct termios ttystate;
        tcgetattr(STDIN_FILENO, &ttystate);
        ttystate.c_lflag |= ICANON;
        tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}

void saveCursorPosition(void) { printf("\033[s"); }

void restoreCursorPosition(void) { printf("\033[u"); }

void setDefaultTextColor(void) { printf("\033[0m"); }

int isInputAvailable(void)
{
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0;

        int ret = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);

        if (ret < 0)
        {
                return 0;
        }
        int result = (ret > 0) && (FD_ISSET(STDIN_FILENO, &fds));
        return result;
}

void hideCursor(void) { printf("\033[?25l"); }

void showCursor(void)
{
        printf("\033[?25h");
        fflush(stdout);
}

void resetConsole(void)
{
        // Print ANSI escape codes to reset terminal, clear screen, and move
        // cursor to top-left
        printf("\033\143");     // Reset to Initial State (RIS)
        printf("\033[3J");      // Clear scrollback buffer
        printf("\033[H\033[J"); // Move cursor to top-left and clear screen

        fflush(stdout);
}

void clearRestOfScreen(void) { printf("\033[J"); }

void clearLine(void) { printf("\033[2K"); }

void clearRestOfLine(void) { printf("\033[K"); }

void clearScreen(void)
{
        printf("\033[3J");              // Clear scrollback buffer
        printf("\033[2J\033[3J\033[H"); // Move cursor to top-left and clear
                                        // screen and scrollback buffer
}

void gotoFirstLineFirstRow(void) { printf("\033[H"); }

void enableScrolling(void) { printf("\033[?7h"); }

void disableTerminalLineInput(void) { setvbuf(stdout, NULL, _IOFBF, BUFSIZ); }

void setRawInputMode(void)
{
        struct termios term;
        tcgetattr(STDIN_FILENO, &term);
        term.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &term);
}

void enableInputBuffering()
{
        struct termios term;
        tcgetattr(STDIN_FILENO, &term);
        term.c_lflag |= ICANON | ECHO;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &term);
}

void cursorJump(int numRows)
{
        if (numRows < 0 || numRows > MAX_TERMINAL_ROWS)
                return;

        printf("\033[%dA", numRows);
        printf("\033[0m");
}

void cursorJumpDown(int numRows)
{
        if (numRows < 0 || numRows > MAX_TERMINAL_ROWS)
                return;

        printf("\033[%dB", numRows);
}

int readInputSequence(char *seq, size_t seqSize)
{
        if (seq == NULL ||
            seqSize < 2) // Buffer needs at least 1 byte + null terminator
                return 0;

        char c;
        ssize_t bytesRead = read(STDIN_FILENO, &c, 1);
        if (bytesRead <= 0)
                return 0;

        // ASCII character (single byte, no continuation bytes)
        if ((c & 0x80) == 0)
        {
                if (seqSize <
                    2) // Make sure there's space for the null terminator
                        return 0;
                seq[0] = c;
                seq[1] = '\0';
                return 1;
        }

        // Determine the length of the UTF-8 sequence and validate the first
        // byte
        int additionalBytes;
        if ((c & 0xE0) == 0xC0)
                additionalBytes = 1; // 2-byte sequence
        else if ((c & 0xF0) == 0xE0)
                additionalBytes = 2; // 3-byte sequence
        else if ((c & 0xF8) == 0xF0)
                additionalBytes = 3; // 4-byte sequence
        else
                return 0; // Invalid UTF-8 start byte

        if ((size_t)(additionalBytes + 1) >= seqSize)
                return 0;

        seq[0] = c;

        // Read the continuation bytes
        bytesRead = read(STDIN_FILENO, &seq[1], additionalBytes);
        if (bytesRead != additionalBytes)
                return 0;

        // Validate continuation bytes (0x80 <= byte <= 0xBF)
        for (int i = 1; i <= additionalBytes; ++i)
        {
                if ((seq[i] & 0xC0) != 0x80)
                        return 0; // Invalid continuation byte
        }

        // Null terminate the string
        seq[additionalBytes + 1] = '\0';

        return additionalBytes +
               1; // Return the total length including the null terminator
}

int getIndentation(int textWidth)
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);

        if (textWidth <= 0 || term_w <= 0)
        {
                return 0;
        }

        if (textWidth >= term_w)
        {
                textWidth = term_w;
        }

        int available_space = term_w - textWidth;
        int indent = (available_space / 2) + 1;

        return indent;
}

void enterAlternateScreenBuffer(void)
{
        // Enter alternate screen buffer
        printf("\033[?1049h");
}

void exitAlternateScreenBuffer(void)
{
        // Exit alternate screen buffer
        printf("\033[?1049l");
}

void enableTerminalMouseButtons(void)
{
        // Enable program to accept mouse input as codes
        printf("\033[?1002h\033[?1006h");
}

void disableTerminalMouseButtons(void)
{
        // Disable program to accept mouse input as codes
        printf("\033[?1002l\033[?1006l");
}

void setTerminalWindowTitle(char *title)
{
        // Only change window title, no icon
        printf("\033]2;%s\007", title);
}

void saveTerminalWindowTitle(void)
{
        // Save terminal window title on the stack
        printf("\033[22;0t");
}

void restoreTerminalWindowTitle(void)
{
        // Restore terminal window title from the stack
        printf("\033[23;0t");
}
