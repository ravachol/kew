/**
 * @file common.h
 * @brief Provides common function such as errorr message handling.
 *
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>

typedef enum {
        k_unknown = 0,
        k_aac = 1,
        k_rawAAC = 2, // Raw aac (.aac file) decoding is included here for convenience although they are not .m4a files
        k_ALAC = 3,
        k_FLAC = 4
} k_m4adec_filetype;

/**
 * Sets the current error message.
 *
 * Copies the provided message into the internal error buffer.
 * If the message is NULL, no action is taken. The error is marked
 * as not yet printed and a refresh is triggered.
 *
 * @param message  The error message to set (must not be NULL)
 */
void set_error_message(const char *message);

/**
 * Checks whether an error message is currently set.
 *
 * @return true if an error message exists,
 *         false if the error buffer is empty
 */
bool has_error_message(void);

/**
 * Clears the current error message.
 *
 * Resets the internal error buffer to an empty string.
 */
void clear_error_message(void);

/**
 * Marks the current error message as printed.
 *
 * Sets the internal flag indicating that the error
 * message has already been displayed.
 */
void mark_error_message_as_printed(void);

/**
 * Triggers a screen refresh.
 *
 * Sets the internal refresh flag to true, indicating that
 * a refresh operation should be performed.
 */
void trigger_refresh(void);


/**
 * Cancels a pending screen refresh.
 *
 * Sets the internal refresh flag to false, preventing
 * a refresh operation from occurring.
 */
void cancel_refresh(void);

/**
 * Checks whether a refresh has been triggered.
 *
 * @return true if a refresh is currently triggered,
 *         false otherwise
 */
bool is_refresh_triggered(void);

/**
 * Checks whether the current error message has already been printed.
 *
 * @return true if the error message has been printed,
 *         false otherwise
 */
bool has_printed_error_message(void);


/**
 * Returns the current error message.
 *
 * @return Pointer to the internal error message buffer.
 *         The returned pointer must not be freed or modified.
 */
char *get_error_message(void);

#endif
