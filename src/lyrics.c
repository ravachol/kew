#include "lyrics.h"
#include "utils.h"
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const int maxLength = 512;

Lyrics *loadLyrics(const char *path)
{
        if (!path)
                return NULL;

        char lrcPath[PATH_MAX];
        if (snprintf(lrcPath, sizeof(lrcPath), "%s", path) >= (int)sizeof(lrcPath))
                return NULL; // path too long

        char *dot = strrchr(lrcPath, '.');
        if (!dot || dot == lrcPath)
                return NULL;

        if (snprintf(dot, sizeof(lrcPath) - (dot - lrcPath), ".lrc") >= (int)(sizeof(lrcPath) - (dot - lrcPath)))
                return NULL;

        FILE *file = fopen(lrcPath, "r");
        if (!file)
                return NULL;

        Lyrics *lyrics = calloc(1, sizeof(Lyrics));
        if (!lyrics)
        {
                fclose(file);
                return NULL;
        }

        lyrics->maxLength = 1024; // define or pass as parameter
        size_t capacity = 60;
        lyrics->lines = malloc(sizeof(LyricsLine) * capacity);

        if (!lyrics->lines)
        {
                free(lyrics);
                fclose(file);
                return NULL;
        }

        char lineBuffer[1024];

        while (fgets(lineBuffer, sizeof(lineBuffer), file))
        {
                if (lineBuffer[0] != '[' || !isdigit((unsigned char)lineBuffer[1]))
                        continue;

                int min = 0, sec = 0, cs = 0;
                char text[512] = {0};

                if (sscanf(lineBuffer, "[%d:%d.%d]%511[^\r\n]", &min, &sec, &cs, text) == 4)
                {
                        if (lyrics->count == capacity)
                        {
                                size_t newCapacity = capacity * 2;
                                LyricsLine *newLines = realloc(lyrics->lines, sizeof(LyricsLine) * newCapacity);

                                if (!newLines)
                                {
                                        freeLyrics(lyrics);
                                        fclose(file);
                                        return NULL;
                                }

                                lyrics->lines = newLines;
                                capacity = newCapacity;
                        }

                        // Sanitize text (trim leading/trailing spaces)
                        char *start = text;

                        while (isspace((unsigned char)*start))
                                start++;

                        char *end = start + strlen(start);

                        while (end > start && isspace((unsigned char)*(end - 1)))
                                *(--end) = '\0';

                        lyrics->lines[lyrics->count].timestamp = min * 60.0 + sec + cs / 100.0;
                        lyrics->lines[lyrics->count].text = strdup(start);

                        if (!lyrics->lines[lyrics->count].text)
                        {
                                freeLyrics(lyrics);
                                fclose(file);
                                return NULL;
                        }

                        lyrics->count++;
                }
        }

        fclose(file);
        return lyrics;
}

void freeLyrics(Lyrics *lyrics)
{
        if (!lyrics)
        {
                return;
        }
        for (size_t i = 0; i < lyrics->count; i++)
        {
                free(lyrics->lines[i].text);
        }
        free(lyrics->lines);
        free(lyrics);
}

char *getLyricsLine(Lyrics *lyrics, double elapsed_seconds)
{
        if (!lyrics || lyrics->count == 0)
        {
                return "";
        }

        char *line = "";

        for (size_t i = 0; i < lyrics->count; i++)
        {
                if (elapsed_seconds >= lyrics->lines[i].timestamp)
                {
                        line = lyrics->lines[i].text;
                }
                else
                {
                        break;
                }
        }

        return line;
}
