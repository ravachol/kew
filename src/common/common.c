/**
 * @file common.h
 * @brief Provides common function such as errorr message handling.
 *
 */

#include "common.h"

#include <pthread.h>
#include <string.h>

#define ERROR_MESSAGE_LENGTH 256

static char current_error_message[ERROR_MESSAGE_LENGTH];
static bool has_printed_error = true;
static volatile bool refresh_triggered = true;

void trigger_refresh(void) { refresh_triggered = true; }

void cancel_refresh(void) { refresh_triggered = false; }

bool is_refresh_triggered(void) { return (refresh_triggered == true); }

void set_error_message(const char *message)
{
        if (message == NULL)
                return;

        strncpy(current_error_message, message, ERROR_MESSAGE_LENGTH - 1);
        current_error_message[ERROR_MESSAGE_LENGTH - 1] = '\0';
        has_printed_error = false;
        trigger_refresh();
}

bool has_printed_error_message(void) { return has_printed_error; }

bool has_error_message(void) { return (current_error_message[0] != '\0'); }

void mark_error_message_as_printed(void) { has_printed_error = true; }

char *get_error_message(void) { return current_error_message; }

void clear_error_message(void) { current_error_message[0] = '\0'; }
