/**
 * @file input.h
 * @brief Handles keyboard and terminal input events.
 */

#include "common/appstate.h"
#include "common/events.h"

struct Event processInput(void);
bool is_digits_pressed(void);
char *get_digits_pressed(void);
void reset_digits_pressed(void);
void update_last_input_time(void);
void init_key_mappings(AppSettings *settings);
void press_digit(int digit);
void init_input(void);
void shutdown_input(void);
void handle_cooldown(void);
void cycle_visualization(void);
