/**
 * @file cli.h
 * @brief Handles command-line related functions
 *
 * Contains the function that shows the welcome screen and sets the path for the first time.
 */

 #include <stdbool.h>

void removeArgElement(char *argv[], int index, int *argc);
void handleOptions(int *argc, char *argv[], bool *exactSearch);
void setMusicPath(void);
