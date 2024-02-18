#include "cutils.h"
/*

cutils.c

 Utility functions for instance for replacing some standard functions with safer alterantives.
 
*/
void c_sleep(int milliseconds)
{
        struct timespec ts;
        ts.tv_sec = milliseconds / 1000;
        ts.tv_nsec = milliseconds % 1000 * 1000000;
        nanosleep(&ts, NULL);
}

void c_usleep(int microseconds)
{
        struct timespec ts;
        ts.tv_sec = microseconds / 1000000;
        ts.tv_nsec = microseconds % 1000;
        nanosleep(&ts, NULL);
}

void c_strcpy(char *dest, size_t dest_size, const char *src)
{
        if (dest && dest_size > 0 && src)
        {
                size_t src_length = strlen(src);
                if (src_length < dest_size)
                {
                        strncpy(dest, src, dest_size);
                }
                else
                {
                        strncpy(dest, src, dest_size - 1);
                        dest[dest_size - 1] = '\0';
                }
        }
}
