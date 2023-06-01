#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

#define ANSI_COLOR_CLEARLINE "\x1b[2K\r"
#define ANSI_COLOR_WHITE "\x1b[37m"
#define ANSI_COLOR_RESET "\x1b[0m"
#define ANSI_GET_CURSOR_POS "\033[6n"
#define ANSI_SET_CURSOR_POS "\033[%d;%dH"

void get_cursor_position_nonblocking(int *row, int *col) {
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

    char input;
    int count = 0;
    while (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0) {
        if (read(STDIN_FILENO, &input, 1) == 1) {
            if (input == '\033') {  // Escape character
                count++;
                if (count >= 2) {
                    break;  // Escape sequence received
                }
            } else if (count > 0) {
                if (input == '[' || input == ';') {
                    count = 0;
                } else if (isdigit(input)) {
                    // Parse row and column values from the input
                    *row = (*row * 10) + (input - '0');
                } else if (input == 'R') {
                    break;  // Cursor position received
                }
            }
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &term_orig);
}


// get cursor position (blocks input)
void get_cursor_position(int *row, int *col) {
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

// Function to enable non-blocking keyboard input
void enableRawMode() {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void setTextColorRGB(int r, int g, int b)
{
    printf("\033[1;38;2;%03u;%03u;%03um", r, g, b);
}

void disableRawMode() {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void get_term_size(int *width, int *height) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    *height = (int)w.ws_row;
    *width = (int)w.ws_col;
}

// Function to set terminal to non-blocking mode
void setNonblockingMode() {
  struct termios ttystate;
  tcgetattr(STDIN_FILENO, &ttystate);
  ttystate.c_lflag &= ~ICANON;
  ttystate.c_cc[VMIN] = 0;
  ttystate.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}

// Function to restore terminal to normal mode
void restoreTerminalMode() {
  struct termios ttystate;
  tcgetattr(STDIN_FILENO, &ttystate);
  ttystate.c_lflag |= ICANON;
  tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}

// Function to check if stdin has any available input
bool isInputAvailable() {
  struct pollfd fds[1];
  fds[0].fd = STDIN_FILENO;
  fds[0].events = POLLIN;

  int ret = poll(fds, 1, 0);
  return ret > 0 && (fds[0].revents & POLLIN);
}

// Function to read a single character from stdin
char readInput() {
  char c;
  read(STDIN_FILENO, &c, 1);
  return c;
}

void save_cursor_position() {
    printf("\033[s");  // Save cursor position
    fflush(stdout);
}

void restore_cursor_position() {
    printf("\033[u");  // Restore cursor position
    fflush(stdout);
}

void set_cursor_position(int row, int col) {
    printf("\033[%d;%dH", row, col);  // Set cursor position
    fflush(stdout);
}

void clear_screen() {
    printf("\033[J");  // Clear screen from cursor to end of screen
    fflush(stdout);
}

int getCurrentLine() {
    printf("\033[6n"); // Send the cursor position query
    fflush(stdout);

    char response[32];
    ssize_t bytesRead = read(STDIN_FILENO, response, sizeof(response) - 1);
    response[bytesRead] = '\0';

    int line = 0;
    sscanf(response, "\033[%d", &line);

    return line;
}

int get_cursor_line() {
    struct winsize size;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == -1) {        
        return 0;
    }
    return size.ws_row;
}

int getCurrentLinePosition() {
    printf("\033[6n"); // Send the cursor position query

    // Read the response from the terminal
    char buffer[32];
    ssize_t bytesRead = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
    if (bytesRead <= 0) {
        perror("Failed to read cursor position");
        return -1;
    }
    buffer[bytesRead] = '\0';

    // Parse the response to extract the line number
    int line = 0;
    sscanf(buffer, "\033[%dR", &line);

    return line;
}

void moveCursorToLastLine() {
    printf("\033[999B");  // Move the cursor 999 lines down
}

void enableScrolling() {
    printf("\033[?7h");  // Enable line wrapping (scrolling)
}

void disableScrolling() {
    printf("\033[?7l");  // Disable line wrapping (scrolling)
}