
#include "common_ui.h"

/*

common_ui.c

 UI functions.

*/

//0=Black, 1=Red, 2=Green, 3=Yellow, 4=Blue, 5=Magenta, 6=Cyan, 7=White

void setTextColorRGB2(int r, int g, int b, UISettings *ui)
{
        if (!ui->useConfigColors)
                setTextColorRGB(r, g, b);
}

void setColor(UISettings *ui)
{
    setColorAndWeight(0, ui);
}

void setColorAndWeight(int bold, UISettings *ui)
{
        if (ui->useConfigColors)
        {
                printf("\033[%dm", bold);
                return;
        }
        if (ui->color.r == defaultColor && ui->color.g == defaultColor && ui->color.b == defaultColor)
                printf("\033[%dm", bold);
        else if (ui->color.r >= 210 && ui->color.g >= 210 && ui->color.b >= 210)
        {
                ui->color.r = defaultColor;
                ui->color.g = defaultColor;
                ui->color.b = defaultColor;
                printf("\033[%d;38;2;%03u;%03u;%03um", bold, ui->color.r, ui->color.g, ui->color.b);
        }
        else
        {
                printf("\033[%d;38;2;%03u;%03u;%03um", bold, ui->color.r, ui->color.g, ui->color.b);
        }
}
