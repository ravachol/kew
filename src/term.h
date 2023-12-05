#ifndef TERM_H
#define TERM_H
#ifndef __USE_POSIX
#define __USE_POSIX
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 2
#endif
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
#include <stdbool.h>

#ifdef __GNU__
# define _BSD_SOURCE
#endif

#define ANSI_COLOR_CLEARLINE "\x1b[2K\r"
#define ANSI_COLOR_WHITE "\x1b[37m"
#define ANSI_COLOR_RESET "\x1b[0m"
#define ANSI_GET_CURSOR_POS "\033[6n"
#define ANSI_SET_CURSOR_POS "\033[%d;%dH"

extern volatile sig_atomic_t resizeFlag;
extern bool useProfileColors;

void getTermSizePixels(int *width, int *height);

void getCursorPosition(int *row, int *col);

void getCursorPosition2(int *row, int *col);

void enableRawMode(void);

void set_blocking_mode(int fd, int should_block);

void setTextColor(int color);

void setTextColorRGB(int r, int g, int b);

void setWindowTitle(const char *title);

void disableRawMode(void);

void getTermSize(int *width, int *height);

void setDefaultTextColor(void);

void setNonblockingMode(void);

void restoreTerminalMode(void);

int isInputAvailable(void);

void saveCursorPosition(void);

void restoreCursorPosition(void);

void setCursorPosition(int row, int col);

void hideCursor(void);

void showCursor(void);

void clearRestOfScreen(void);

void enableScrolling(void);

void disableScrolling(void);

int getFirstLineRow(void);

int getVisibleFirstLineRow(void);

void handleResize(int sig);

void resetResizeFlag(int sig);

void initResize(void);

void disableInputBuffering(void);

void enableInputBuffering(void);

void cursorJump(int numRows);

void cursorJumpDown(int numRows);

void clearScreen(void);

int readInputSequence(char *seq, size_t seqSize);

#endif
