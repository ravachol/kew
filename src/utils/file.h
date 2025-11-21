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

const char *get_temp_dir();
void get_directory_from_path(const char *path, char *directory);
void collapse_path(const char *input, char *output);
void generate_temp_file_path(char *file_path, const char *prefix, const char *suffix);
int directory_exists(const char *path);
int is_directory(const char *path);
int walker(const char *start_path, const char *searching, char *result,
           const char *allowed_extensions, enum SearchType search_type, bool exact_search, int depth);
int expand_path(const char *input_path, char *expanded_path);
int create_directory(const char *path);
int delete_file(const char *file_path);
int is_in_temp_dir(const char *path);
int exists_file(const char *fname);

#endif
