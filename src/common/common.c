/**
 * @file common.h
 * @brief Provides common function such as errorr message handling.
 *
 */

#include "common/common.h"
#include <pthread.h>
#include <string.h>

#define ERROR_MESSAGE_LENGTH 256

static char currentErrorMessage[ERROR_MESSAGE_LENGTH];
static bool hasPrintedError = true;
static volatile bool refreshTriggered = true;

void triggerRefresh(void) { refreshTriggered = true; }

void cancelRefresh(void) { refreshTriggered = false; }

bool isRefreshTriggered(void) { return (refreshTriggered == true); }

void setErrorMessage(const char *message)
{
        if (message == NULL)
                return;

        strncpy(currentErrorMessage, message, ERROR_MESSAGE_LENGTH - 1);
        currentErrorMessage[ERROR_MESSAGE_LENGTH - 1] = '\0';
        hasPrintedError = false;
        triggerRefresh();
}

bool hasPrintedErrorMessage(void) { return hasPrintedError; }

bool hasErrorMessage(void) { return (currentErrorMessage[0] != '\0'); }

void markErrorMessageAsPrinted(void) { hasPrintedError = true; }

char *getErrorMessage(void) { return currentErrorMessage; }

void clearErrorMessage(void) { currentErrorMessage[0] = '\0'; }
