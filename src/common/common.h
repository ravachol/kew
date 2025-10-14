/**
 * @file common.h
 * @brief Provides common function such as errorr message handling.
 *
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

typedef enum
{
        k_unknown = 0,
        k_aac = 1,
        k_rawAAC = 2, // Raw aac (.aac file) decoding is included here for convenience although they are not .m4a files
        k_ALAC = 3,
        k_FLAC = 4
} k_m4adec_filetype;

void setErrorMessage(const char *message);
bool hasErrorMessage(void);
char *getErrorMessage(void);
void clearErrorMessage(void);
void markErrorMessageAsPrinted(void);
void triggerRefresh(void);
void cancelRefresh(void);
bool isRefreshTriggered(void);
bool hasPrintedErrorMessage(void);

#endif
