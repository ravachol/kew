#ifndef COMMON_UI_H
#define COMMON_UI_H
#include <stdbool.h>
#include "appstate.h"
#include "term.h"
#include "common.h"

extern unsigned int updateCounter;

extern const int scrollingInterval;
extern bool isSameNameAsLastTime;

void setTextColorRGB2(int r, int g, int b, UISettings *ui);

void setColor(UISettings *ui);

void setColorAndWeight(int bold, UISettings *ui);

void processNameScroll(const char *name, char *output, int maxWidth, int isSameNameAsLastTime);

void resetNameScroll();

bool getIsLongName();

void processName(const char *name, char *output, int maxWidth);

#endif
