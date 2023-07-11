
#include "term.h"

volatile sig_atomic_t resizeFlag = 0;

void getTermSizePixels(int *rows, int *columns) {
    FILE *fp = popen("stty size", "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to execute stty command\n");
        return;
    }

    if (fscanf(fp, "%d %d", rows, columns) != 2) {
        fprintf(stderr, "Failed to read terminal size\n");
        pclose(fp);
        return;
    }

    pclose(fp);
}

char* queryTerminalProperty(int property) {
    // Build the query string
    char query[32];
    snprintf(query, sizeof(query), "\033[%d;?\033\\", property);

    // Save the terminal settings
    struct termios oldTermios;
    tcgetattr(STDIN_FILENO, &oldTermios);

    // Set terminal to raw mode
    struct termios newTermios = oldTermios;
    newTermios.c_lflag &= ~(ICANON | ECHO);
    newTermios.c_cc[VMIN] = 0;
    newTermios.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newTermios);

    // Send the query
    printf("%s", query);
    fflush(stdout);

    // Read the response
    char answer[128];
    ssize_t bytesRead = read(STDIN_FILENO, answer, sizeof(answer));

    // Restore terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);

    // Process the response
    char* result = NULL;
    if (bytesRead > 0) {
        answer[bytesRead] = '\0';
        char* start = strchr(answer, ';');
        if (start != NULL)
            result = strdup(start + 1);
    }

    return result;
}

char *getVariableValue(const char *variableName)
{
    char *value = getenv(variableName);
    if (value == NULL)
    {
        return "";
    }

    return value;
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
        scanf("\033[%d;%dR", row, col);
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &term_orig);
}

void getCursorPosition2(int *row, int *col)
{
    printf("\033[6n");
    fflush(stdout);
    scanf("\033[%d;%dR", row, col);
}

void enableRawMode()
{
    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void setTextColorRGB(int r, int g, int b)
{
    printf("\033[38;2;%03u;%03u;%03um", r, g, b);
}

void setTextColorRGB2(int r, int g, int b)
{
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
    fflush(stdout);
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

int isInputAvailable()
{
    while (read(STDIN_FILENO, NULL, 0) > 0)
        ;

    struct pollfd fds[1];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    int ret = poll(fds, 1, 0);
    if (ret < 0)
    {
        return 0;
    }

    return ret > 0 && (fds[0].revents & POLLIN);
}

void set_blocking_mode(int fd, int should_block) {
    struct termios tty;
    tcgetattr(fd, &tty);
    if (should_block) {
        tty.c_lflag |= ICANON;
    } else {
        tty.c_lflag &= ~ICANON;
    }
    tcsetattr(fd, TCSANOW, &tty);
}

char readInput()
{
    char c;
    ssize_t bytesRead = read(STDIN_FILENO, &c, 1);

    if (bytesRead == 0)
    {
    }
    else if (bytesRead == -1)
    {
    }
    else
    {
        return c;
    }
    return '\0';
}

void saveCursorPosition()
{
    printf("\033[s");
    fflush(stdout);
}

void restoreCursorPosition()
{
    printf("\033[u");
    fflush(stdout);
}

void setCursorPosition(int row, int col)
{
    printf("\033[%d;%dH", row, col);
    fflush(stdout);
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
    fflush(stdout);
}

void clearScreen()
{
    printf("\033[2J");
    printf("\033[H");
}

void setWindowTitle(const char *title)
{
    printf("\033]0;%s\007", title);
    fflush(stdout);    
}

int getCurrentLine()
{
    printf("\033[6n");
    fflush(stdout);

    char response[32];
    ssize_t bytesRead = read(STDIN_FILENO, response, sizeof(response) - 1);
    response[bytesRead] = '\0';

    int line = 0;
    sscanf(response, "\033[%d", &line);

    return line;
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
    scanf("\033[%d;%dR", &row, &col);

    int visibleFirstLineRow = row - (ws.ws_row - 1);
    return visibleFirstLineRow;
}

void handleResize(int sig)
{
    resizeFlag = 1;
}

void resetResizeFlag(int sig)
{
    resizeFlag = 0;
}

// Disable input buffering
void disableInputBuffering()
{
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &term);
}

// Enable input buffering
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
    fflush(stdout);   
}

void cursorJumpDown(int numRows) {
    printf("\033[%dB", numRows);
    fflush(stdout);
}