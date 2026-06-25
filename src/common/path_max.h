#include <limits.h>

#ifdef _WIN32
#define KEW_PATH_MAX 32768
#else
#ifndef PATH_MAX
// Internal application path buffer size.
// This is not the Windows MAX_PATH limit.
// Windows Unicode APIs can handle paths longer than 260 characters.
#if defined(__APPLE__)
#define KEW_PATH_MAX 1024
#else
#define KEW_PATH_MAX 4096
#endif
#endif
#endif


