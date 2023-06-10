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

void getCursorPosition(int *row, int *col);

void enableRawMode();

void setTextColorRGB(int r, int g, int b);

void disableRawMode();

void getTermSize(int *width, int *height);

void setDefaultTextColor();

void setNonblockingMode();

void restoreTerminalMode();

bool isInputAvailable();

char readInput();

void saveCursorPosition();

void restoreCursorPosition();

void setCursorPosition(int row, int col);

void clearRestOfScreen();

int getCurrentLine();

void enableScrolling();

void disableScrolling();