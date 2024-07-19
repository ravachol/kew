
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
        if (useProfileColors)
        {
                setDefaultTextColor();
                return;
        }
        if (color.r == 150 && color.g == 150 && color.b == 150)
                setDefaultTextColor();
        else if (color.r >= 210 && color.g >= 210 && color.b >= 210)
        {
                color.r = defaultColor;
                color.g = defaultColor;
                color.b = defaultColor;
        }
        else
        {
                setTextColorRGB2(color.r, color.g, color.b);
        }
}
