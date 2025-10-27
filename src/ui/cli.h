/**
 * @file cli.h
 * @brief Handles command-line related functions
 *
 * Contains the function that shows the welcome screen and sets the path for the first time.
 */

 #include <stdbool.h>

void remove_arg_element(char *argv[], int index, int *argc);
void handle_options(int *argc, char *argv[], bool *exactSearch);
void set_music_path(void);
