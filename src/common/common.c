/**
 * @file common.h
 * @brief Provides common function such as errorr message handling.
 *
 */

#include "common.h"
#include "common/appstate.h"
#include "common/model.h"
#include "utils/utils.h"

#include <string.h>
#include <signal.h>

#define ERROR_MESSAGE_LENGTH 256

static char current_error_message[ERROR_MESSAGE_LENGTH];
static bool has_printed_error = true;
static volatile sig_atomic_t g_should_exit = 0;


bool is_refresh_triggered(void)
{
        Model *model = get_model();
        return (model->dirty == DIRTY_ALL);
}

void set_error_message(const char *message)
{
        if (message == NULL)
                return;

        c_strcpy(current_error_message, message, ERROR_MESSAGE_LENGTH - 1);
        current_error_message[ERROR_MESSAGE_LENGTH - 1] = '\0';
        has_printed_error = false;

        set_dirty(DIRTY_FOOTER);
}

bool has_printed_error_message(void)
{
        return has_printed_error;
}

bool has_error_message(void)
{
        return (current_error_message[0] != '\0');
}

void mark_error_message_as_printed(void)
{
        has_printed_error = true;
}

char *get_error_message(void)
{
        return current_error_message;
}

void clear_error_message(void)
{
        current_error_message[0] = '\0';
}

sig_atomic_t should_exit(void)
{
        return g_should_exit;
}

void quit(void)
{
        g_should_exit = 1;
}

void handle_exit_signal(int sig)
{
        (void)sig;
        g_should_exit = 1;
}
