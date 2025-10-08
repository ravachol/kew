#include "file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fnmatch.h>
#include <libgen.h>
#include <limits.h>
#include <time.h>


int existsFile(const char *fname) {
    return access(fname, F_OK) == 0;
}


Lyrics *load_lyrics(const char *music_file_path) {
    if (!music_file_path) return NULL;
    char lyric_path[MAXPATHLEN];
    strncpy(lyric_path, music_file_path, MAXPATHLEN - 1);
    char *ext = strrchr(lyric_path, '.');
    if (ext) {
        strcpy(ext, ".lrc");
    }
    if (!existsFile(lyric_path)) {
        ext = strrchr(lyric_path, '.');
        if (ext) {
            strcpy(ext, ".txt");
        }
    }

    FILE *fp = fopen(lyric_path, "r");
    if (!fp) return NULL;

    Lyrics *lyrics = malloc(sizeof(Lyrics));
    lyrics->lines = NULL;
    lyrics->count = 0;

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        if (line[0] == '[') {
            float time = 0.0f;
            char *text = strchr(line, ']');
            if (text) {
                text++;
                if (sscanf(line, "[%f]", &time) == 1) {
                    lyrics->lines = realloc(lyrics->lines, (lyrics->count + 1) * sizeof(LyricLine));
                    lyrics->lines[lyrics->count].time = time;
                    lyrics->lines[lyrics->count].text = strdup(text);
                    lyrics->count++;
                }
            }
        } else if (strlen(line) > 0) {
            lyrics->lines = realloc(lyrics->lines, (lyrics->count + 1) * sizeof(LyricLine));
            lyrics->lines[lyrics->count].time = 0.0f;
            lyrics->lines[lyrics->count].text = strdup(line);
            lyrics->count++;
        }
    }

    fclose(fp);
    return lyrics;
}

void free_lyrics(Lyrics *lyrics) {
    if (!lyrics) return;

    for (int i = 0; i < lyrics->count; i++) {
        free(lyrics->lines[i].text);
    }
    free(lyrics->lines);
    free(lyrics);
}

// Implementasi fungsi-fungsi lain dari file.h

void getDirectoryFromPath(const char *path, char *directory) {
    char *copy = strdup(path);
    char *dir = dirname(copy);
    strncpy(directory, dir, MAXPATHLEN - 1);
    directory[MAXPATHLEN - 1] = '\0';
    free(copy);
}

int isDirectory(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return 1;
    }
    return 0;
}

int walker(const char *startPath, const char *searching, char *result,
           const char *allowedExtensions, enum SearchType searchType, bool exactSearch, int depth) {
    if (depth < 0) return -1;

    DIR *dir = opendir(startPath);
    if (!dir) return -1;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char fullpath[MAXPATHLEN];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", startPath, entry->d_name);

        if (entry->d_type == DT_DIR) {
            if (searchType == DirOnly || searchType == SearchPlayList) {
                if (strcmp(entry->d_name, searching) == 0) {
                    strcpy(result, fullpath);
                    closedir(dir);
                    return 0;
                }
            }
            if (searchType != DirOnly) {
                if (walker(fullpath, searching, result, allowedExtensions, searchType, exactSearch, depth - 1) == 0) {
                    closedir(dir);
                    return 0;
                }
            }
        } else if (entry->d_type == DT_REG) {
            if (searchType == FileOnly || searchType == SearchAny || searchType == SearchPlayList || searchType == ReturnAllSongs) {
                if (allowedExtensions) {
                    if (fnmatch(allowedExtensions, entry->d_name, 0) == 0) {
                        if (strcmp(entry->d_name, searching) == 0) {
                            strcpy(result, fullpath);
                            closedir(dir);
                            return 0;
                        }
                    }
                }
            }
        }
    }
    closedir(dir);
    return -1;
}

int expandPath(const char *inputPath, char *expandedPath) {
    if (inputPath[0] == '~') {
        const char *home = getenv("HOME");
        if (home) {
            snprintf(expandedPath, MAXPATHLEN, "%s%s", home, inputPath + 1);
            return 0;
        }
    }
    strncpy(expandedPath, inputPath, MAXPATHLEN - 1);
    expandedPath[MAXPATHLEN - 1] = '\0';
    return 0;
}

int createDirectory(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        if (mkdir(path, 0755) != 0) {
            return -1;
        }
    }
    return 0;
}

int deleteFile(const char *filePath) {
    return unlink(filePath);
}

void generateTempFilePath(char *filePath, const char *prefix, const char *suffix) {
    char temp_dir[] = "/tmp/kew_temp_XXXXXX";
    char *tmp_dir = mkdtemp(temp_dir);
    snprintf(filePath, MAXPATHLEN, "%s/%s%s%s", tmp_dir, prefix, "temp", suffix);
}

int isInTempDir(const char *path) {
    if (strncmp(path, "/tmp/", 5) == 0) {
        return 1;
    }
    return 0;
}