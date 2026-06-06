/*
 * @file settings.h
 * @brief Application settings management.
 *
 * Loads, saves, and applies configuration settings such as
 * playback behavior, UI preferences, and library paths.
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include "common/events.h"
#include "common/model.h"

#define NUM_DEFAULT_KEY_BINDINGS 55

extern size_t keybinding_count;

/**
 * @brief Retrieves the key bindings.
 *
 * This function returns the array of key bindings used in the application.
 *
 * @return A pointer to an array of key bindings.
 */
TBKeyBinding *get_key_bindings(void);

/**
 * @brief Retrieves the application's configuration settings.
 *
 * This function reads the configuration file and populates the given settings and UI settings
 * with the values from the configuration file.
 *
 * @param settings Pointer to the AppSettings structure to be populated.
 * @param ui Pointer to the UISettings structure to be populated.
 */
void get_config(AppSettings *settings, UISettings *ui);

/**
 * @brief Retrieves the user's preferences.
 *
 * This function reads the preferences file and updates the provided settings and UI settings
 * with values from the preferences file.
 *
 * @param settings Pointer to the AppSettings structure to be populated.
 * @param ui Pointer to the UISettings structure to be populated.
 */
void get_prefs(AppSettings *settings, UISettings *ui);

/**
 * @brief Saves the application's configuration settings.
 *
 * This function writes the current application settings to the configuration file.
 *
 * @param settings Pointer to the AppSettings structure containing the settings to be saved.
 * @param ui Pointer to the UISettings structure containing the UI settings to be saved.
 */
void set_config(AppSettings *settings, UISettings *ui);

/**
 * @brief Saves the user's preferences.
 *
 * This function writes the current preferences to the preferences file.
 *
 * @param settings Pointer to the AppSettings structure containing the settings to be saved.
 * @param ui Pointer to the UISettings structure containing the UI settings to be saved.
 */
void set_prefs(AppSettings *settings, UISettings *ui);

/**
 * @brief Sets the path for the configuration or preferences files.
 *
 * This function updates the path configuration in the application settings.
 *
 * @param path The path to set.
 */
void set_path(const char *path);

/**
 * @brief Maps the application's settings to event mappings.
 *
 * This function maps various settings (such as scroll actions or track control) to event types.
 *
 * @param settings Pointer to the AppSettings structure containing the settings to map.
 * @param mappings Array to store the resulting event mappings.
 */
void map_settings_to_keys(AppSettings *settings, EventMapping *mappings);

/**
 * @brief Updates the value of a key in the runtime configuration file.
 *
 * This function reads the configuration file, updates the specified key with a new value, and writes
 * the updated contents back to the file.
 *
 * @param path The path to the configuration file.
 * @param key The key to update.
 * @param value The new value for the key.
 * @return 0 on success, -1 on failure.
 */
int update_rc(const char *path, const char *key, const char *value);

/**
 * @brief Retrieves the string representation of a key binding for a specific event.
 *
 * This function returns a string representation of the key binding for a specified event type,
 * formatted as a human-readable key combination.
 *
 * @param event The event for which to retrieve the binding string.
 * @param find_only_one If true, stops after finding the first match; otherwise, returns all matches.
 * @return A string representing the key binding for the event.
 */
const char *get_binding_string(enum MsgType event, bool find_only_one);

/**
 * @brief Initializes the application settings by loading configuration and preferences.
 *
 * This function initializes the application settings by loading both the configuration and preferences
 * for the application and user interface.
 *
 * @param settings
 *
 * @return The initialized AppSettings structure.
 */
void init_settings(AppSettings *settings);

/**
 * @brief Loads the layout config from file
 *
 * This function loads the layout config file, that contains the different layouts for the views.
 *
 */
void load_layout_config(void);

/**
 * @brief Frees the layout config from memory.
 *
 */
void free_layout_config(void);

/**
 * @brief Returns true if a layout section with section name exists in memory.
 *
 * @param section The name of the section.
 * @return True if the section was found.
 */
bool config_has_section(const char *section);

/**
 * @brief Loads the layout config from file
 *
 * This function loads the layout for an individual view, from the config file in memory.
 * load_layout_config() should first be called.
 *
 * @param layout_name
 * @return a layout.
 */
Layout *load_layout_from_config(const char *layout_name);

/**
 * @brief Copies the layouts to the users config folder, if needed.
 *
 * @return True of the layouts have changed.
 */
bool ensure_default_layouts(void);

#endif
