#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "common.h"

const char VERSION[] = "3.5.1";

const char LAST_ROW[] = " [F2 Playlist|F3 Library|F4 Track|F5 Search|F6 Help]";

const int MOUSE_DRAG = 32;
const int MOUSE_CLICK = 0;

double pauseSeconds = 0.0;
double totalPauseSeconds = 0.0;
double seekAccumulatedSeconds = 0.0;

#define ERROR_MESSAGE_LENGTH 256

char currentErrorMessage[ERROR_MESSAGE_LENGTH];

bool hasPrintedError = true;

volatile bool refresh = true; // Should the whole view be refreshed next time it redraws

void setErrorMessage(const char *message)
{
        if (message == NULL)
                return;

        strncpy(currentErrorMessage, message, ERROR_MESSAGE_LENGTH - 1);
        currentErrorMessage[ERROR_MESSAGE_LENGTH - 1] = '\0';
        hasPrintedError = false;
        refresh = true;
}

bool hasErrorMessage()
{
        return (currentErrorMessage[0] != '\0');
}

char *getErrorMessage()
{
        return currentErrorMessage;
}

void clearErrorMessage()
{
        currentErrorMessage[0] = '\0';
}
