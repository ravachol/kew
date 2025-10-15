/**
 * @file theme.h
 * @brief Loads themes.
 *
 * Loads themes, and copies the themes to config dir.
 */

#include "common/appstate.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

int hexToPixel(const char *hex, PixelData *result);
int loadThemeFromFile(const char *themesDir, const char *filename, Theme *currentTheme);
bool ensureDefaultThemes(void);
