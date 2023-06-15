
#include "term.h"

char* getVariableValue(const char* variableName) 
{
    char* value = getenv(variableName);
    if (value == NULL) {       
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
    tv.tv_usec = 10000; // 10ms

    if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0) {
        scanf("\033[%d;%dR", row, col);
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &term_orig);
}

void getCursorPosition2(int *row, int *col) {
    printf("\033[6n");  // Request cursor position report
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
    printf("\033[1;38;2;%03u;%03u;%03um", r, g, b);
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

bool isInputAvailable() 
{
  struct pollfd fds[1];
  fds[0].fd = STDIN_FILENO;
  fds[0].events = POLLIN;

  int ret = poll(fds, 1, 0);
  return ret > 0 && (fds[0].revents & POLLIN);
}

char readInput() 
{
  char c;
  read(STDIN_FILENO, &c, 1);
  return c;
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

void hideCursor() {
    printf("\033[?25l");  // Hide the cursor
    fflush(stdout);
}

void showCursor() {
    printf("\033[?25h");  // Show the cursor
    fflush(stdout);
}


void clearRestOfScreen() 
{
    printf("\033[J");
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

int getFirstLineRow() {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
        perror("Failed to get terminal window size");
        return -1;
    }
    return ws.ws_row;
}

int getVisibleFirstLineRow() {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
        perror("Failed to get terminal window size");
        return -1;
    }

    // Get the current cursor position
    printf("\033[6n"); // Send escape sequence to query cursor position
    fflush(stdout);

    int row, col;
    scanf("\033[%d;%dR", &row, &col); // Read the cursor position response

    int visibleFirstLineRow = row - (ws.ws_row - 1);
    return visibleFirstLineRow;
}