/**
 * @file file.h
 * @brief File and directory utilities.
 *
 * Provides wrappers around file I/O, path manipulation,
 * and safe filesystem access used throughout the app.
 */

#ifndef FILE_H
#define FILE_H

#include <stdbool.h>

#define __USE_GNU

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define MUSIC_FILE_EXTENSIONS "(m4a|aac|mp3|ogg|flac|wav|opus|webm)$"

#define AUDIO_EXTENSIONS "(m4a|aac|mp3|ogg|flac|wav|opus|webm|m3u|m3u8)$"

enum SearchType {
        SearchAny = 0,
        DirOnly = 1,
        FileOnly = 2,
        SearchPlayList = 3,
        ReturnAllSongs = 4
};

/**
 * @brief Retrieves the path to the temporary directory.
 *
 * This function checks the environment variables `TMPDIR` and `TEMP` to determine the
 * location of the temporary directory. If neither is set, it defaults to `/tmp` on Unix-like systems.
 *
 * @return The path to the temporary directory as a string.
 */
const char *get_temp_dir(void);


/**
 * @brief Extracts the directory part of a given path.
 *
 * This function extracts the directory path from a given file path. It ensures that
 * the result ends with a trailing '/' if necessary.
 *
 * @param path The file path to extract the directory from.
 * @param directory The buffer to store the extracted directory path.
 */
void get_directory_from_path(const char *path, char *directory);


/**
 * @brief Collapses a path by replacing the home directory with `~`.
 *
 * This function collapses a given path, replacing the user's home directory with `~`
 * if the path starts with the home directory. If the path is not under the home directory,
 * it remains unchanged.
 *
 * @param input The input file path to collapse.
 * @param output The buffer to store the collapsed path.
 */
void collapse_path(const char *input, char *output);


/**
 * @brief Generates a temporary file path.
 *
 * This function generates a temporary file path by combining a provided prefix and suffix
 * with a randomly generated string. The path is placed in a subdirectory under the system's
 * temporary directory.
 *
 * @param file_path The buffer to store the generated file path.
 * @param prefix The prefix to be added at the beginning of the file path.
 * @param suffix The suffix to be added at the end of the file path.
 */
void generate_temp_file_path(char *file_path, const char *prefix, const char *suffix);


/**
 * @brief Checks if a directory exists at the given path.
 *
 * This function checks if a directory exists at the specified path by attempting to open
 * the directory. It returns `1` if the directory exists, `0` if it doesn't, and `-1` if there
 * is an error.
 *
 * @param path The path to check for the existence of a directory.
 *
 * @return `1` if the directory exists, `0` if it does not exist, and `-1` if there is an error.
 */
int directory_exists(const char *path);


/**
 * @brief Checks if the given path is a directory.
 *
 * This function checks if the given path refers to a directory by attempting to open
 * the path as a directory. If the path is valid and a directory, it returns `1`, if
 * it's a file, it returns `0`, and if the path is invalid, it returns `-1`.
 *
 * @param path The path to check.
 *
 * @return `1` if the path is a directory, `0` if it's not, and `-1` if there's an error.
 */
int is_directory(const char *path);


/**
 * @brief Traverses a directory tree and searches for a specific file or directory.
 *
 * This function recursively traverses directories starting from `start_path`, searching for
 * a file or directory matching the search string (`low_case_searching`). It also supports filtering
 * by file extensions and depth limits.
 *
 * @param start_path The path to start the search from.
 * @param searching The string to search for (case insensitive).
 * @param result The buffer to store the result path when found.
 * @param allowed_extensions A regular expression to match allowed file extensions.
 * @param search_type The type of search (e.g., directory or file search).
 * @param exact_search Flag indicating whether to use an exact search (false uses partial match).
 * @param depth The current recursion depth.
 *
 * @return `0` if the search is successful, `1` if not found or if an error occurs.
 */
int walker(const char *start_path, const char *searching, char *result,
           const char *allowed_extensions, enum SearchType search_type, bool exact_search, int depth);


/**
 * @brief Expands a file path by resolving the `~` and relative paths.
 *
 * This function expands the given file path by resolving `~` to the home directory and
 * making any other necessary adjustments to create an absolute path.
 *
 * @param input_path The input path to expand.
 * @param expanded_path The buffer to store the expanded absolute path.
 *
 * @return `0` if expansion is successful, `-1` if an error occurs.
 */
int expand_path(const char *input_path, char *expanded_path);


/**
 * @brief Creates a directory at the specified path.
 *
 * This function attempts to create a directory at the given path. It first checks if
 * the directory already exists and returns appropriate responses.
 *
 * @param path The path where the directory should be created.
 *
 * @return `1` if the directory is successfully created, `0` if it already exists, and `-1` if there is an error.
 */
int create_directory(const char *path);


/**
 * @brief Deletes a file at the specified path.
 *
 * This function deletes the file at the given path. If the operation is successful,
 * it returns `0`, otherwise it returns `-1`.
 *
 * @param file_path The path of the file to delete.
 *
 * @return `0` if the file is successfully deleted, `-1` if there is an error.
 */
int delete_file(const char *file_path);


/**
 * @brief Checks if the given path is within the temporary directory.
 *
 * This function checks if the specified path lies within the system's temporary directory.
 *
 * @param path The path to check.
 *
 * @return `1` if the path is within the temporary directory, `0` otherwise.
 */
int is_in_temp_dir(const char *path);


/**
 * @brief Checks if a file exists at the given path.
 *
 * This function checks if a file exists at the specified path by attempting to open it in read mode.
 * It returns `1` if the file exists, and `-1` if it does not.
 *
 * @param fname The file path to check.
 *
 * @return `1` if the file exists, `-1` if it does not exist.
 */
int exists_file(const char *fname);

#endif
