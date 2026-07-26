/**
 * @file lyrics.cpp
 * @brief Lyrics fetching and parsing.
 *
 * Provides functions to load, cache lyrics from local files
 * or remote sources. Supports synchronized lyric display and fallback modes
 * when metadata or network access is unavailable.
 */

#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lyrics.h"

#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/mpegfile.h>
#include <taglib/oggflacfile.h>
#include <taglib/opusfile.h>
#include <taglib/synchronizedlyricsframe.h>
#include <taglib/tag.h>
#include <taglib/tstringlist.h>
#include <taglib/vorbisfile.h>
#include <taglib/xiphcomment.h>

// LRC Compare function
static bool compareLyricsLine(const LyricsLine &a, const LyricsLine &b)
{
        return a.timestamp < b.timestamp;
}

char* parseTimestamp(char* lyricSlice, double* timestamp, char openerChar) {
    char formatString[64];
    snprintf(formatString, 63, "%c%s:%s%s", openerChar, "%d", "%d", "%n");

    int min = 0, sec = 0, n = 0;
    if (sscanf(lyricSlice,formatString, &min, &sec, &n) == 2) {
            char *ptr = lyricSlice + n;
            if (*ptr == '\0') return NULL;
            double frac = 0.0;
            if (*ptr == '.') {
                    ptr++;
                    double pad = 0.1;
                    while (isdigit((unsigned char)*ptr)) {
                            frac += (*ptr - '0') * pad;
                            pad /= 10.0;
                            ptr++;
                    }
            }

            *timestamp = min * 60.0 + sec + frac;
            return ptr;
    }

    return NULL;
}

int parseKaraokeLine(char* ptr, Lyrics* lyrics, double firstStamp) {
        double timestampArr[METADATA_MAX_LENGTH] = {0};
        int numberOfTimestamps = 0;
        char karaokeString[256] = {0};
        char *start = ptr;
        char *end;
        double timestamp = 0.0f;

        if (*ptr != '<') {
            ptr = strchr(start, '<');
            if (ptr == NULL) ptr = start;
            else {
                strncat(karaokeString, start, ptr - start);
                timestampArr[numberOfTimestamps++] = firstStamp;
            }
        }

        while (*ptr == '<') {
            ptr = parseTimestamp(ptr, &timestamp, '<');
            if (ptr == NULL) continue;
            timestampArr[numberOfTimestamps++] = timestamp;
            lyrics->isKaraoke = 1;
            if (*ptr == '>') ptr++;

            end = strchr(ptr, '<');

            if (end == NULL)
                end = ptr + strlen(ptr);

            strncat(karaokeString, ptr, end - ptr);

            ptr = end;
        }

        if (karaokeString[0] != '\0') {
            lyrics->lines[lyrics->count].text = strdup(karaokeString);
        }

        if (numberOfTimestamps > 0) {
                for (int i = 0; i < numberOfTimestamps &&
                                i < METADATA_MAX_LENGTH;
                     i++
                ) {
                        lyrics->lines[lyrics->count].timestampArray[i] =
                                                        timestampArr[i];
                }
                lyrics->lines[lyrics->count].numberOfTimestamps = numberOfTimestamps;
        }
        else {
                lyrics->lines[lyrics->count].numberOfTimestamps = 0;
        }

        return numberOfTimestamps;
}

int parseTimedLyricsLine(char* line, Lyrics* lyrics, size_t* lyricsCapacity) {
        if (lyrics == NULL) {
                return 0;
        }
        if (lyrics->lines == NULL) {
                lyrics->lines = (LyricsLine *)malloc(sizeof(LyricsLine) * (*lyricsCapacity));
        }

        int numberOfTimestamps = 0;
        double timestamp = 0.0f;
        if (line[0] != '[' || !isdigit((unsigned char)line[1]))
                return 0;

        char* ptr = parseTimestamp(line,
                                   &timestamp,
                                   '['
                    );
        if (ptr == NULL) return 0;
        if (*ptr == ']') {
                ptr++;
                if (lyrics->count == *lyricsCapacity) {
                        (*lyricsCapacity) *= 2;
                        LyricsLine *newLines = (LyricsLine *)realloc(lyrics->lines, sizeof(LyricsLine) * (*lyricsCapacity) );

                        if (!newLines) {
                                for (size_t i = 0; i < lyrics->count; i++)
                                        free(lyrics->lines[i].text);
                                free(lyrics->lines);
                                lyrics->lines = NULL;
                                return 0;
                        }

                        lyrics->lines = newLines;
                }

                while (isspace((unsigned char)*ptr))
                        ptr++;
                char *end = ptr + strlen(ptr);


                while (end > ptr && isspace((unsigned char)*(end - 1)))
                        *(--end) = '\0';

                lyrics->lines[lyrics->count].timestamp = timestamp;
                if (strchr(ptr, '<') == NULL) {
                    lyrics->lines[lyrics->count].text = strdup(ptr);
                }
                else {
                    numberOfTimestamps = parseKaraokeLine(ptr, lyrics, timestamp);
                }
                

                if (!lyrics->lines[lyrics->count].text) {
                        freeLyricLines(lyrics);
                        return 0;
                }

                lyrics->count++;
        }

        return numberOfTimestamps + 1; // include the first, non-karaoke timestamp
}

// LRC Loader
static int loadTimedLyrics(FILE *file, Lyrics *lyrics)
{
        size_t capacity = 64;
        lyrics->lines = (LyricsLine *)malloc(sizeof(LyricsLine) * capacity);
        if (!lyrics->lines)
                return 0;

        char lineBuffer[1024];

        while (fgets(lineBuffer, sizeof(lineBuffer), file)) {
            parseTimedLyricsLine(lineBuffer, lyrics, &capacity);
        }
        std::stable_sort(lyrics->lines, lyrics->lines + lyrics->count, compareLyricsLine);

        lyrics->isTimed = 1;
        return 1;
}

static int loadUntimedLyrics(FILE *file, Lyrics *lyrics)
{
        size_t capacity = 64;
        lyrics->lines = (LyricsLine *)malloc(sizeof(LyricsLine) * capacity);
        if (!lyrics->lines)
                return 0;

        char lineBuffer[1024];
        lyrics->count = 0;

        while (fgets(lineBuffer, sizeof(lineBuffer), file)) {
                char *newline = strpbrk(lineBuffer, "\r\n");
                if (newline)
                        *newline = '\0';

                if (lineBuffer[0] == '\0')
                        continue;

                if (lyrics->count == capacity) {
                        capacity *= 2;
                        LyricsLine *newLines = (LyricsLine *)realloc(lyrics->lines, sizeof(LyricsLine) * capacity);
                        if (!newLines)
                                return 0;
                        lyrics->lines = newLines;
                }

                lyrics->lines[lyrics->count].timestamp = 0.0;
                lyrics->lines[lyrics->count].text = strdup(lineBuffer);
                if (!lyrics->lines[lyrics->count].text)
                        return 0;

                lyrics->count++;
        }

        lyrics->isTimed = 0;
        return 1;
}

//helper function findLRC that find wether current folder have a .lrc file match the word
static char *findLRC(const char *folder, const char *word)
{
        static char result[KEW_PATH_MAX];
        DIR *d = opendir(folder);

        if (!d)
                return NULL;

        struct dirent *e;

        while ((e = readdir(d))) {
                if (!strstr(e->d_name, ".lrc") || !strstr(e->d_name, word))
                        continue;

                if (strlen(folder) + strlen(e->d_name) + 2 > sizeof(result))
                        continue;

                strcpy(result, folder);
                strcat(result, "/");
                strcat(result, e->d_name);

                closedir(d);
                return result;
        }

        closedir(d);
        return NULL;
}

Lyrics *loadLyricsFromLRC(const char *path,SongData *songdata)
{
        char lrcPath[KEW_PATH_MAX];
        if (snprintf(lrcPath, sizeof(lrcPath), "%s", path) >= (int)sizeof(lrcPath))
                return nullptr;

        char corrTitle[255];
        strcpy(corrTitle,songdata->metadata->title);

        for (size_t i=0; i<strlen(corrTitle); i++){
                if (corrTitle[i] == '/'){
                        corrTitle[i] = '-';
                }
        }

        char *dot = strrchr(lrcPath, '.');
        if (!dot || dot == lrcPath)
                return nullptr;

        if (snprintf(dot, sizeof(lrcPath) - (dot - lrcPath), ".lrc") >= (int)(sizeof(lrcPath) - (dot - lrcPath)))
                return nullptr;

        FILE *file = fopen(lrcPath, "r");
        if (!file){
                // try to substring match a lrc using title of metadata of songdata
                char *slash = strrchr(lrcPath, '/');
                if (!slash || slash == lrcPath)
                        return nullptr;

                char curFolder[KEW_PATH_MAX];
                if (snprintf(curFolder, slash - lrcPath + 1, "%s", lrcPath) >= KEW_PATH_MAX)
                        return nullptr;

                char *fallbackPath = findLRC(curFolder, corrTitle);

                if (!fallbackPath) {
                        return nullptr;
                }

                file = fopen(fallbackPath, "r");
                if (!file) {
                        fprintf(stderr, "Failed to open lyrics: %s\n", fallbackPath);
                        return nullptr;
                }
        }
        Lyrics *lyrics = (Lyrics *)calloc(1, sizeof(Lyrics));

        if (!lyrics) {
                fclose(file);
                return nullptr;
        }

        lyrics->max_length = 1024;

        // Detect if there are timestamps
        char lineBuffer[1024];
        int foundTimestamp = 0;
        while (fgets(lineBuffer, sizeof(lineBuffer), file)) {
                if (lineBuffer[0] == '[' && isdigit((unsigned char)lineBuffer[1])) {
                        foundTimestamp = 1;
                        break;
                }
        }

        rewind(file);

        int ok = foundTimestamp ? loadTimedLyrics(file, lyrics) : loadUntimedLyrics(file, lyrics);

        fclose(file);

        if (!ok) {
                freeLyrics(lyrics);
                return nullptr;
        }

        return lyrics;
}

void freeLyricLines(Lyrics *lyrics) {
        if (!lyrics)
                return;
        for (size_t i = 0; i < lyrics->count; i++)
                free(lyrics->lines[i].text);
        free(lyrics->lines);
        lyrics->lines = NULL;
}

// Free & Access
void freeLyrics(Lyrics *lyrics)
{
        if (!lyrics)
                return;
        for (size_t i = 0; i < lyrics->count; i++)
                free(lyrics->lines[i].text);
        free(lyrics->lines);
        free(lyrics);
}
