#include <limits.h>

#ifndef KEW_PATH_MAX

#ifdef _WIN32

// Windows APIs can support longer paths with Unicode APIs.
// This is an internal buffer size, not the MAX_PATH limit.
#define KEW_PATH_MAX 32768

#else

#ifdef PATH_MAX

#define KEW_PATH_MAX PATH_MAX

#else

// Fallback internal buffer size when PATH_MAX is unavailable.
#if defined(__APPLE__)
#define KEW_PATH_MAX 1024
#else
#define KEW_PATH_MAX 4096
#endif

#endif /* PATH_MAX */

#endif /* _WIN32 */

#endif /* KEW_PATH_MAX */

