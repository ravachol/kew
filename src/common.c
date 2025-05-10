#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "common.h"

const char VERSION[] = "3.3.0";

#define ERROR_MESSAGE_LENGTH 256

char currentErrorMessage[ERROR_MESSAGE_LENGTH];

bool hasPrintedError = true;

volatile bool refresh = true; // Should the whole view be refreshed next time it redraws

void setErrorMessage(const char *message)
{
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
