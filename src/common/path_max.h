#include <limits.h>

#ifndef PATH_MAX
  // If PATH_MAX is not defined, define it based on the platform. On macOS, PATH_MAX is typically 1024, while on Linux it's often 4096.
  #ifdef __APPLE__
    #define PATH_MAX 1024
   #else
    #define PATH_MAX 4096
  #endif
#endif