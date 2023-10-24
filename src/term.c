#include "term.h"
/*

term.c

 This file should contain only simple utility functions related to the terminal. 
 They should work independently and be as decoupled from the application as possible.

*/

bool useProfileColors = true;

volatile sig_atomic_t resizeFlag = 0;

void getTermSizePixels(int *rows, int *columns)
{
    FILE *fp = popen("stty size", "r");
    if (fp == NULL)
    {
        fprintf(stderr, "Failed to execute stty command\n");
        return;
    }

    if (fscanf(fp, "%d %d", rows, columns) != 2)
    {
        fprintf(stderr, "Failed to read terminal size\n");
        pclose(fp);
        return;
    }

    pclose(fp);
}

void getCursorPosition(int *row, int *col)
{
    printf(ANSI_GET_CURSOR_POS);
    fflush(stdout);

    struct termios term, term_orig;
    tcgetattr(STDIN_FILENO, &term);
    term_orig = term;
    term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 10000;

    if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0)
    {
        int scanres = scanf("\033[%d;%dR", row, col);
        (void)scanres;
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &term_orig);
}

void getCursorPosition2(int *row, int *col)
{
    printf("\033[6n");
    fflush(stdout);
    int scanres = scanf("\033[%d;%dR", row, col);
    (void)scanres;
}

void enableRawMode()
{
    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

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

void setTextColorRGB2(int r, int g, int b)
{
    if (!useProfileColors)
        printf("\033[0;38;2;%03u;%03u;%03um", r, g, b);
}

void disableRawMode()
{
    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void getTermSize(int *width, int *height)
{
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    *height = (int)w.ws_row;
    *width = (int)w.ws_col;
}

void setDefaultTextColor()
{
    printf("\033[0m");
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

void set_blocking_mode(int fd, int should_block)
{
    struct termios tty;
    tcgetattr(fd, &tty);
    if (should_block)
    {
        tty.c_lflag |= ICANON;
    }
    else
    {
        tty.c_lflag &= ~ICANON;
    }
    tcsetattr(fd, TCSANOW, &tty);
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

void saveCursorPosition()
{
    printf("\033[s");
}

void restoreCursorPosition()
{
    printf("\033[u");
}

void setCursorPosition(int row, int col)
{
    printf("\033[%d;%dH", row, col);
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

void setWindowTitle(const char *title)
{
    printf("\033]0;%s\007", title);
    fflush(stdout);
}

void enableScrolling()
{
    printf("\033[?7h");
}

void disableScrolling()
{
    printf("\033[?7l");
}

int getFirstLineRow()
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1)
    {
        perror("Failed to get terminal window size");
        return -1;
    }
    return ws.ws_row;
}

int getVisibleFirstLineRow()
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1)
    {
        perror("Failed to get terminal window size");
        return -1;
    }
    printf("\033[6n");
    fflush(stdout);

    int row, col;
    int scanres = scanf("\033[%d;%dR", &row, &col);
    (void)scanres;

    int visibleFirstLineRow = row - (ws.ws_row - 1);
    return visibleFirstLineRow;
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

void disableInputBuffering()
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
    if (bytesRead <= 0 & seq[0] == '\0')
    {
        seq[0] = '\0';
        return 0;
    }

    seq[bytesRead] = '\0';
    return bytesRead;
}