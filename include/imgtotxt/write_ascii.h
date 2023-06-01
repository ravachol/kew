#ifndef C_H
#define C_H

/* This ifdef allows the header to be used from both C and C++. */
#ifdef __cplusplus
extern "C" {
#endif
int output_ascii(char* pathToImgFile, int height, int width);
#ifdef __cplusplus
}
#endif

#endif
