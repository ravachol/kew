#include "lyrics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h> 
#include "utils.h" 

Lyrics *load_lyrics(const char *music_file_path) {
    if (!music_file_path) {
        return NULL;
    }

    char lrc_path[PATH_MAX];
    c_strcpy(lrc_path, music_file_path, sizeof(lrc_path));
    char *dot = strrchr(lrc_path, '.');
    if (dot == NULL || dot == lrc_path) {
        return NULL; 
    }
    strcpy(dot, ".lrc");

    FILE *file = fopen(lrc_path, "r");
    if (!file) {
        return NULL; 
    }

    Lyrics *lyrics = malloc(sizeof(Lyrics));
    if (!lyrics) {
        fclose(file);
        return NULL;
    }
    lyrics->lines = NULL;
    lyrics->count = 0;

    char line_buffer[512];
    while (fgets(line_buffer, sizeof(line_buffer), file)) {
        // Format parsing: [mm:ss.xx]Text Lirik
        if (line_buffer[0] == '[' && isdigit(line_buffer[1])) {
            int min, sec, cs;
            char text[256] = {0};
            if (sscanf(line_buffer, "[%d:%d.%d]%[^\r\n]", &min, &sec, &cs, text) == 4) {
                lyrics->count++;
                LyricLine *new_lines = realloc(lyrics->lines, sizeof(LyricLine) * lyrics->count);
                if (!new_lines) {
                    free_lyrics(lyrics);
                    fclose(file);
                    return NULL;
                }
                lyrics->lines = new_lines;

                lyrics->lines[lyrics->count - 1].timestamp = min * 60.0 + sec + cs / 100.0;
                lyrics->lines[lyrics->count - 1].text = strdup(text);
            }
        }
    }

    fclose(file);
    return lyrics;
}

void free_lyrics(Lyrics *lyrics) {
    if (!lyrics) {
        return;
    }
    for (size_t i = 0; i < lyrics->count; i++) {
        free(lyrics->lines[i].text);
    }
    free(lyrics->lines);
    free(lyrics);
}

const char *get_current_lyric_line(const Lyrics *lyrics, double elapsed_seconds) {
    if (!lyrics || lyrics->count == 0) {
        return ""; 
    }
    const char *current_line = "";
    for (size_t i = 0; i < lyrics->count; i++) {
        if (elapsed_seconds >= lyrics->lines[i].timestamp) {
            current_line = lyrics->lines[i].text;
        } else {
            break;
        }
    }
    return current_line;
}