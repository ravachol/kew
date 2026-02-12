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

/**
 * @brief Loads a theme configuration from a specified theme file.
 *
 * This function attempts to load a theme configuration from the provided theme
 * directory and filename. It reads the theme file, parses key-value pairs, and
 * applies them to the provided `current_theme` structure. The theme file must
 * contain settings for various UI colors (e.g., accent, text, header) and other
 * parameters, with each setting stored in the appropriate field of the `Theme`
 * structure.
 *
 * The theme file is expected to have key-value pairs, where the keys correspond
 * to fields in the `Theme` structure. For example, the key "accent" will map
 * to the `current_theme->accent` field. Color values are expected to be in a
 * format that can be parsed by the `parse_color_value` function.
 *
 * @param themes_dir Directory containing theme files.
 * @param filename The name of the theme file to load.
 * @param current_theme Pointer to the `Theme` structure to store the loaded
 *                      theme configuration.
 *
 * @return 1 if the theme file was successfully loaded and parsed, 0 if there
 *         was an error (e.g., file not found, parsing error, etc.).
 *
 * @note The function initializes the `current_theme` to default values before
 *       loading the theme data. It returns 0 if the theme file cannot be read
 *       or if any critical fields are missing or malformed.
 */
int load_theme_from_file(const char *themes_dir, const char *filename, Theme *current_theme);

/**
 * @brief Ensures that the default theme files are available in the user's theme directory.
 *
 * This function checks if the default themes exist in the specified configuration
 * directory. If not, it creates the necessary directory and copies the default
 * theme files from the system theme directory to the user's theme directory.
 * The function ensures that the user's theme directory contains up-to-date theme
 * files, copying any newer system themes that are not yet present.
 *
 * The system theme directory is expected to be at a predefined location
 * (e.g., `/share/kew/themes`), and the function copies files with the `.theme`
 * or `.txt` extension. The copy process is based on modification timestamps,
 * so only newer files are copied.
 *
 * @return `true` if at least one theme file was copied or updated, `false` if
 *         no copying was needed or if an error occurred.
 *
 * @note The function assumes that the configuration path is already available
 *       and valid. It will attempt to create the theme directory if it does not
 *       exist.
 */
bool ensure_default_themes(void);
