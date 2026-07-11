#ifndef KEW_BUILD_CONFIG_H
#define KEW_BUILD_CONFIG_H

/* Turns an unquoted -D value into a real string literal at compile time,
 * so the Makefile never has to pass literal quote characters through the
 * shell to gcc. Works identically on Linux, macOS, and MSYS2/MinGW. */
#define _KEW_STR(x) #x
#define _KEW_XSTR(x) _KEW_STR(x)

#ifdef KEW_VERSION_RAW
#define KEW_VERSION _KEW_XSTR(KEW_VERSION_RAW)
#endif

#ifdef KEW_DATADIR_RAW
#define KEW_DATADIR _KEW_XSTR(KEW_DATADIR_RAW)
#endif

#ifdef PREFIX_RAW
#define PREFIX _KEW_XSTR(PREFIX_RAW)
#endif

#ifdef LOCALEDIR_RAW
#define LOCALEDIR _KEW_XSTR(LOCALEDIR_RAW)
#endif

#endif /* KEW_BUILD_CONFIG_H */
