#include "term.h"

/*

term.c

 This file should contain only simple utility functions related to the terminal.
 They should work independently and be as decoupled from the application as possible.

*/

volatile sig_atomic_t resizeFlag = 0;

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
        int status = system("reset");
        if (status == -1) {                
                perror("system() failed");        
        }
}

void clearRestOfScreen()
{
        printf("\033[J");
}

void clearScreen()
{
        printf("\033[2J");
        printf("\033[H");
        printf("\033[1;1H");
}

void enableScrolling()
{
        printf("\033[?7h");
}

void handleResize(int sig)
{
        (void)sig;
        resizeFlag = 1;
}

void resetResizeFlag(int sig)
{
        (void)sig;
        resizeFlag = 0;
}

void initResize()
{
        signal(SIGWINCH, handleResize);

        struct sigaction sa;
        sa.sa_handler = resetResizeFlag;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGALRM, &sa, NULL);
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

        // If it's not an escape character, return it as a single-character sequence
        if (c != '\x1b')
        {
                seq[0] = c;
                seq[1] = '\0';
                return 1;
        }

        // Read additional characters
        if (seqSize < 3)
                return 0;

        bytesRead = read(STDIN_FILENO, seq, 2);
        if ((bytesRead <= 0) & (seq[0] == '\0'))
        {
                seq[0] = '\0';
                return 0;
        }

        seq[bytesRead] = '\0';
        return bytesRead;
}
