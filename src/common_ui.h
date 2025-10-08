#ifndef COMMON_UI_H
#define COMMON_UI_H

#include <stdbool.h>
#include "appstate.h"

void setRGB(PixelData p);

void setAlbumColor(PixelData color);

int getUpdateCounter(void);

void incrementUpdateCounter(void);

void inverseText(void);

void applyColor(ColorMode mode, ColorValue themeColor, PixelData albumColor);

void processNameScroll(const char *name, char *output, int maxWidth, bool isSameNameAsLastTime);

void resetNameScroll(void);

void resetColor(void);

bool getIsLongName(void);

void processName(const char *name, char *output, int maxWidth, bool stripUnneededChars, bool stripSuffix);

PixelData increaseLuminosity(PixelData pixel, int amount);

PixelData decreaseLuminosityPct(PixelData base, float pct);

PixelData getGradientColor(PixelData baseColor, int row, int maxListSize, int startGradient, float minPct);

#endif
