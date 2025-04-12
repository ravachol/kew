
#include "common_ui.h"

/*

common_ui.c

 UI functions.

*/

unsigned int updateCounter = 0;

// Name scrolling
bool finishedScrolling = false;
int lastNamePosition = 0;
bool isLongName = false;
int scrollDelaySkippedCount = 0;
bool isSameNameAsLastTime = false;
const int startScrollingDelay = 10; // Delay before beginning to scroll. 64ms * scrollingInterval * startScrollingDelay = delay in ms
const int scrollingInterval = 1;    // Interval between scrolling updates

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

void resetNameScroll()
{
        lastNamePosition = 0;
        isLongName = false;
        finishedScrolling = false;
        scrollDelaySkippedCount = 0;
}

void processName(const char *name, char *output, int maxWidth)
{
        char *lastDot = strrchr(name, '.');
        int copyLength = (lastDot != NULL) ? lastDot - name : maxWidth;

        if (copyLength > maxWidth)
                copyLength = maxWidth;

        if (copyLength < 0)
                copyLength = 0;

        c_strcpy(output, name, copyLength + 1);

        output[copyLength] = '\0';
        removeUnneededChars(output, copyLength);
        trim(output, copyLength);
}

void processNameScroll(const char *name, char *output, int maxWidth, int isSameNameAsLastTime)
{
        const char *lastDot = strrchr(name, '.');
        size_t nameLength = strlen(name);
        size_t scrollableLength = (lastDot != NULL) ? (size_t)(lastDot - name) : nameLength;

        if (scrollDelaySkippedCount <= startScrollingDelay && scrollableLength > (size_t)maxWidth)
        {
                scrollableLength = maxWidth;
                scrollDelaySkippedCount++;
                refresh = true;
                isLongName = true;
        }

        int start = (isSameNameAsLastTime) ? lastNamePosition : 0;

        if (finishedScrolling)
                scrollableLength = maxWidth;

        if (scrollableLength <= (size_t)maxWidth || finishedScrolling)
        {
                strncpy(output, name, scrollableLength);
                output[scrollableLength] = '\0';
                start = 0;

                removeUnneededChars(output, scrollableLength);
                trim(output, scrollableLength);
        }
        else
        {
                isLongName = true;

                if ((size_t)(start + maxWidth) > scrollableLength)
                {
                        start = 0;
                        finishedScrolling = true;
                }

                strncpy(output, name + start, maxWidth);
                output[maxWidth] = '\0';

                removeUnneededChars(output, maxWidth);
                trim(output, maxWidth);

                lastNamePosition++;

                refresh = true;
        }
}



bool getIsLongName()
{
        return isLongName;
}
