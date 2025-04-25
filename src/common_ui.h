#ifndef COMMON_UI_H
#define COMMON_UI_H
#include <stdbool.h>
#include <locale.h>
#include <wchar.h>
#include "appstate.h"
#include "term.h"
#include "common.h"

extern unsigned int updateCounter;

extern const int scrollingInterval;
extern bool isSameNameAsLastTime;

void setTextColorRGB2(int r, int g, int b, UISettings *ui);

void setColor(UISettings *ui);

void setColorAndWeight(int bold, PixelData color, int useConfigColors);

void processNameScroll(const char *name, char *output, int maxWidth, bool isSameNameAsLastTime);

void resetNameScroll();

bool getIsLongName();

void processName(const char *name, char *output, int maxWidth);

PixelData increaseLuminosity(PixelData pixel, int amount);

PixelData decreaseLuminosityPct(PixelData base, float pct);

PixelData getGradientColor(PixelData baseColor, int row, int maxListSize, int startGradient, float minPct);

#endif
