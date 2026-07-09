#ifndef ULOG_H
#define ULOG_H

#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifndef ULOG_TAG
#define ULOG_TAG "libmp4"
#endif

#define ULOG_DEBUG 100
#define ULOG_INFO  200
#define ULOG_WARN  300
#define ULOG_ERROR 400

#define ULOG_DECLARE_TAG(tag) \
static const char *ULOG_TAG_NAME __attribute__((unused)) = #tag

#define ULOG_STRINGIFY(x) #x
#define ULOG_TOSTRING(x) ULOG_STRINGIFY(x)

/*
 * Debug/error logging
 */
#define ULOGD(...) do {} while (0)
#define ULOGE(...) do {} while (0)
#define ULOGW(...) do {} while (0)
#define ULOG_PRI(...) do {} while (0)
#define ULOG_ERRNO(...) do {} while (0)

#define ULOG_ERRNO_RETURN_ERR_IF(cond, err) \
do { \
        if (cond) { \
                ULOG_ERRNO(#cond); \
                return -(err); \
        } \
} while (0)


#define ULOG_ERRNO_RETURN_IF(cond, err) \
do { \
        if (cond) { \
                ULOG_ERRNO(#cond); \
                return; \
        } \
} while (0)


#define ULOG_ERRNO_RETURN_VAL_IF(cond, err, val) \
do { \
        if (cond) { \
                ULOG_ERRNO(#cond); \
                return (val); \
        } \
} while (0)


#endif
