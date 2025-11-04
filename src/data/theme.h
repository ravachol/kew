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

int load_theme_from_file(const char *themes_dir, const char *filename, Theme *current_theme);
bool ensure_default_themes(void);
