#ifndef TERM_H
#include <stdio.h>

#define TERM_H
#ifndef __USE_POSIX
#define __USE_POSIX
#endif

#ifdef __GNU__
#define _BSD_SOURCE
#endif

void setTextColor(int color);

void setTextColorRGB(int r, int g, int b);

void getTermSize(int *width, int *height);

int getIndentation(int textWidth);

void setNonblockingMode(void);

void restoreTerminalMode(void);

void setDefaultTextColor(void);

int isInputAvailable(void);

void resetConsole(void);

void saveCursorPosition(void);

void restoreCursorPosition(void);

void hideCursor(void);

void showCursor(void);

void clearRestOfScreen(void);

void enableScrolling(void);

void initResize(void);

void disableInputBuffering(void);

void enableInputBuffering(void);

void cursorJump(int numRows);

void cursorJumpDown(int numRows);

void clearScreen(void);

int readInputSequence(char *seq, size_t seqSize);

void enterAlternateScreenBuffer(void);

void exitAlternateScreenBuffer(void);

void enableTerminalMouseButtons(void);

void disableTerminalMouseButtons(void);

void setTerminalWindowTitle(char *title);

void saveTerminalWindowTitle();

void restoreTerminalWindowTitle();

#endif
