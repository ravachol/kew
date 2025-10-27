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

void set_error_message(const char *message);
bool has_error_message(void);
void clear_error_message(void);
void mark_error_message_as_printed(void);
void trigger_refresh(void);
void cancel_refresh(void);
bool is_refresh_triggered(void);
bool has_printed_error_message(void);
char *get_error_message(void);

#endif
