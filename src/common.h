#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

extern volatile bool refresh;

extern const char VERSION[];

extern bool hasPrintedError;

void setErrorMessage(const char *message);

bool hasErrorMessage();

char *getErrorMessage();

void clearErrorMessage();

#endif
