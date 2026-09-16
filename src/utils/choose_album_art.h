
#ifndef CHOOSE_ALBUM_ART_H
#define CHOOSE_ALBUM_ART_H

/**
 * @file choose_album_art.h
 * @brief Allows picking cover art in subdirectories
 *
 */

#include <stdbool.h>

#define MAX_RECURSION_DEPTH 10

/**
 * @brief find album art matching one of several candidate filenames, optionally searching subdirectories.
 *
 */
char *choose_album_art(const char *dir_path, char **custom_file_name_arr,
                        int arr_size, bool search_sub_dirs);

#endif
