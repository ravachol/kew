#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <pwd.h>
#include <sys/param.h>
#include <fcntl.h>
#include <poll.h>

#define ANSI_COLOR_CLEARLINE "\x1b[2K\r"
#define ANSI_COLOR_WHITE "\x1b[37m"
#define ANSI_COLOR_RESET "\x1b[0m"
#define ANSI_GET_CURSOR_POS "\033[6n"
#define ANSI_SET_CURSOR_POS "\033[%d;%dH"

char* getVariableValue(const char* variableName);

// get cursor position (blocks input)
void get_cursor_position(int *row, int *col);

// Function to enable non-blocking keyboard input
void enableRawMode();

void setTextColorRGB(int r, int g, int b);

void disableRawMode();

void get_term_size(int *width, int *height);
void setDefaultTextColor();

// Function to set terminal to non-blocking mode
void setNonblockingMode();

// Function to restore terminal to normal mode
void restoreTerminalMode();

// Function to check if stdin has any available input
bool isInputAvailable();

// Function to read a single character from stdin
char readInput();

void save_cursor_position();

void restore_cursor_position();

void set_cursor_position(int row, int col);

void clear_screen();

int getCurrentLine();

void enableScrolling();

void disableScrolling();