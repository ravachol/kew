/**
 * @file input.h
 * @brief Handles keyboard and terminal input events.
 */

#include "common/appstate.h"
#include "common/events.h"

/**
 * @brief Checks if any digits have been pressed.
 *
 * This function checks if any digit keys (0-9) have been pressed and stored.
 *
 * @return true if any digits have been pressed, false otherwise.
 */
bool is_digits_pressed(void);

/**
 * @brief Retrieves the string of digits that have been pressed.
 *
 * This function returns the string of digits that have been pressed and stored
 * in the application.
 *
 * @return A string of digits that have been pressed.
 */
char *get_digits_pressed(void);

/**
 * @brief Resets the stored digits that have been pressed.
 *
 * This function clears the array of pressed digits and resets the count.
 */
void reset_digits_pressed(void);

/**
 * @brief Updates the timestamp of the last input.
 *
 * This function updates the timestamp to the current time when the last input
 * occurred, which is typically used for input cooldown purposes.
 */
void update_last_input_time(void);

/**
 * @brief Initializes key mappings for the application.
 *
 * This function sets up the key mappings based on the provided application settings.
 * It maps keys to specific functions or events within the application.
 *
 * @param settings The application settings that include key mapping information.
 */
void init_key_mappings(AppSettings *settings);

/**
 * @brief Presses a digit key.
 *
 * This function stores a single digit that has been pressed and updates the count
 * of pressed digits.
 *
 * @param digit The digit (0-9) that was pressed.
 */
void press_digit(int digit);

/**
 * @brief Initializes the input system.
 *
 * This function sets up the input system, including initializing termbox and
 * enabling necessary input modes (e.g., mouse support).
 */
void init_input(void);

/**
 * @brief Shuts down the input system.
 *
 * This function shuts down the input system, freeing any resources related to input handling.
 */
void shutdown_input(void);

/**
 * @brief Handles the cooldown period for certain actions.
 *
 * This function checks if the cooldown period has elapsed and handles actions like
 * fast-forwarding, rewinding, or seeking when the cooldown is not active.
 */
void handle_cooldown(void);

/**
 * @brief Cycles through the available visualizations.
 *
 * This function cycles through the different visualizations (e.g., for music)
 * that can be displayed in the application.
 */
void cycle_visualization(void);
