
#include "common_ui.h"

//0=Black, 1=Red, 2=Green, 3=Yellow, 4=Blue, 5=Magenta, 6=Cyan, 7=White
int mainColor = 6;
int titleColor = 6;
int artistColor = 6;
int enqueuedColor = 6;
PixelData color = {125, 125, 125};
unsigned char defaultColor = 150;

bool useProfileColors = false;

void setTextColorRGB2(int r, int g, int b)
{
        if (!useProfileColors)
                setTextColorRGB(r, g, b);
}

void setColor()
{
    setColorAndWeight(0);
}

void setColorAndWeight(int bold)
{
        if (useProfileColors)
        {
                printf("\033[%dm", bold);
                return;
        }
        if (color.r == defaultColor && color.g == defaultColor && color.b == defaultColor)
                printf("\033[%dm", bold);
        else if (color.r >= 210 && color.g >= 210 && color.b >= 210)
        {
                color.r = defaultColor;
                color.g = defaultColor;
                color.b = defaultColor;
                printf("\033[%d;38;2;%03u;%03u;%03um", bold, color.r, color.g, color.b);
        }
        else
        {
                printf("\033[%d;38;2;%03u;%03u;%03um", bold, color.r, color.g, color.b);
        }
}
