#include <stdbool.h>
#include "term.h"
#include "../include/imgtotxt/write_ascii.h"

//0=Black, 1=Red, 2=Green, 3=Yellow, 4=Blue, 5=Magenta, 6=Cyan, 7=White
extern int mainColor;
extern int titleColor;
extern int artistColor;
extern int enqueuedColor;
extern unsigned char defaultColor;
extern PixelData color;

extern bool useProfileColors;

void setTextColorRGB2(int r, int g, int b);

void setColor();

void setColorAndWeight(int bold);
