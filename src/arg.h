#ifndef ARG_H
#define ARG_H
#include "stringfunc.h"
#include "player.h"

void removeElement(char *argv[], int index, int *argc);

void handleSwitches(int *argc, char *argv[]);

#endif