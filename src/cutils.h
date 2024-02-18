#ifndef CUTILS_H
#define CUTILS_H
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef __USE_POSIX
#define __USE_POSIX
#endif

#include <time.h>
#include <string.h>

void c_sleep(int milliseconds);

void c_usleep(int microseconds);

void c_strcpy(char *dest, size_t dest_size, const char *src);

#endif
