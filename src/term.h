#ifndef TERM_H
#define TERM_H
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>
#include <pwd.h>
#include <sys/param.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <signal.h>
#include <bits/sigaction.h>

#define ANSI_COLOR_CLEARLINE "\x1b[2K\r"
#define ANSI_COLOR_WHITE "\x1b[37m"
#define ANSI_COLOR_RESET "\x1b[0m"
#define ANSI_GET_CURSOR_POS "\033[6n"
#define ANSI_SET_CURSOR_POS "\033[%d;%dH"

extern volatile sig_atomic_t resizeFlag;

extern char songIndex [4];
extern int digitsPressedCount;
extern bool gotosong;

void getTermSizePixels(int *width, int *height);

char *queryTerminalProperty(int property);

char *getVariableValue(const char *variableName);

void getCursorPosition(int *row, int *col);

void getCursorPosition2(int *row, int *col);

void enableRawMode();

void set_blocking_mode(int fd, int should_block);

void setTextColorRGB(int r, int g, int b);

void setTextColorRGB2(int r, int g, int b);

void setWindowTitle(const char *title);

void disableRawMode();

void getTermSize(int *width, int *height);

void setDefaultTextColor();

void setNonblockingMode();

void restoreTerminalMode();

int isInputAvailable();

char readInput();

void saveCursorPosition();

void restoreCursorPosition();

void setCursorPosition(int row, int col);

void hideCursor();

void showCursor();

void clearRestOfScreen();

int getCurrentLine();

void enableScrolling();

void disableScrolling();

int getFirstLineRow();

int getVisibleFirstLineRow();

void handleResize(int sig);

void resetResizeFlag(int sig);

void initResize();

void disableInputBuffering();

void enableInputBuffering();

void cursorJump(int numRows);

void cursorJumpDown(int numRows);

void clearScreen();

int readInputSequence(char* seq, size_t seqSize);

#endif