#ifndef COMMON_UI_H
#define COMMON_UI_H
#include <stdbool.h>
#include "appstate.h"

extern unsigned int updateCounter;

extern const int scrollingInterval;
extern bool isSameNameAsLastTime;

void setRGB(PixelData p);

void setAlbumColor(PixelData color);

void inverseText(void);

void applyColor(ColorMode mode, ColorValue themeColor, PixelData albumColor);

void processNameScroll(const char *name, char *output, int maxWidth, bool isSameNameAsLastTime);

void resetNameScroll();

void resetColor();

bool getIsLongName();

void processName(const char *name, char *output, int maxWidth, bool stripUnneededChars, bool stripSuffix);

PixelData increaseLuminosity(PixelData pixel, int amount);

PixelData decreaseLuminosityPct(PixelData base, float pct);

PixelData getGradientColor(PixelData baseColor, int row, int maxListSize, int startGradient, float minPct);

#endif
