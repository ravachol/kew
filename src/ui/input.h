/**
 * @file input.h
 * @brief Handles keyboard and terminal input events.
 */

#include "common/events.h"
#include "common/appstate.h"

struct Event processInput(void);

bool isDigitsPressed(void);

char *getDigitsPressed(void);

void resetDigitsPressed(void);

void updateLastInputTime(void);

void initKeyMappings(AppSettings* settings);

void pressDigit(int digit);
