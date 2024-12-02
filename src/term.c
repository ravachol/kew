#include "term.h"

/*

term.c

 This file should contain only simple utility functions related to the terminal.
 They should work independently and be as decoupled from the application as possible.

*/

void setTextColor(int color)
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
        */
        printf("\033[0;3%dm", color);
}

void setTextColorRGB(int r, int g, int b)
{
        printf("\033[0;38;2;%03u;%03u;%03um", r, g, b);
}

void getTermSize(int *width, int *height)
{
        struct winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

        *height = (int)w.ws_row;
        *width = (int)w.ws_col;
}

void setNonblockingMode()
{
        struct termios ttystate;
        tcgetattr(STDIN_FILENO, &ttystate);
        ttystate.c_lflag &= ~ICANON;
        ttystate.c_cc[VMIN] = 0;
        ttystate.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}

void restoreTerminalMode()
{
        struct termios ttystate;
        tcgetattr(STDIN_FILENO, &ttystate);
        ttystate.c_lflag |= ICANON;
        tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}

void saveCursorPosition()
{
        printf("\033[s");
}

void restoreCursorPosition()
{
        printf("\033[u");
}

void setDefaultTextColor()
{
        printf("\033[0m");
}

int isInputAvailable()
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

void hideCursor()
{
        printf("\033[?25l");
        fflush(stdout);
}

void showCursor()
{
        printf("\033[?25h");
        fflush(stdout);
}

void resetConsole()
{
        // Print ANSI escape codes to reset terminal, clear screen, and move cursor to top-left
        printf("\033\143");     // Reset to Initial State (RIS)
        printf("\033[3J");      // Clear scrollback buffer
        printf("\033[H\033[J"); // Move cursor to top-left and clear screen
        
        fflush(stdout);
}

void clearRestOfScreen()
{
        printf("\033[J");
}

void clearScreen()
{
        printf("\033[3J");      // Clear scrollback buffer
        printf("\033[H\033[J"); // Move cursor to top-left and clear screen
}

void enableScrolling()
{
        printf("\033[?7h");
}

void disableInputBuffering(void)
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
        printf("\033[%dA", numRows);
        printf("\033[0m");
}

void cursorJumpDown(int numRows)
{
        printf("\033[%dB", numRows);
}

int readInputSequence(char *seq, size_t seqSize)
{
        char c;
        ssize_t bytesRead;
        seq[0] = '\0';
        bytesRead = read(STDIN_FILENO, &c, 1);
        if (bytesRead <= 0)
        {
                return 0;
        }

        // If it's a single-byte ASCII character, return it
        if ((c & 0x80) == 0)
        {
                seq[0] = c;
                seq[1] = '\0';
                return 1;
        }

        // Determine the length of the UTF-8 sequence
        int additionalBytes;
        if ((c & 0xE0) == 0xC0)
        {
                additionalBytes = 1; // 2-byte sequence
        }
        else if ((c & 0xF0) == 0xE0)
        {
                additionalBytes = 2; // 3-byte sequence
        }
        else if ((c & 0xF8) == 0xF0)
        {
                additionalBytes = 3; // 4-byte sequence
        }
        else
        {
                // Invalid UTF-8 start byte
                return 0;
        }

        // Ensure there's enough space in the buffer
        if ((size_t)additionalBytes + 1 >= seqSize)
        {
                return 0;
        }

        // Read the remaining bytes of the UTF-8 sequence
        seq[0] = c;
        bytesRead = read(STDIN_FILENO, &seq[1], additionalBytes);
        if (bytesRead != additionalBytes)
        {
                return 0;
        }

        seq[additionalBytes + 1] = '\0';
        return additionalBytes + 1;
}

int getIndentation(int terminalWidth)
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        int indent = ((term_w - terminalWidth) / 2) + 1;
        return (indent > 0) ? indent : 0;
}
