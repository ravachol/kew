/**
 * @file lyrics.cpp
 * @brief Lyrics fetching and parsing.
 *
 * Provides functions to load, cache lyrics from local files
 * or remote sources. Supports synchronized lyric display and fallback modes
 * when metadata or network access is unavailable.
 */

#include "lyrics.h"

#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/synchronizedlyricsframe.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ID3 SYLT Loader
static Lyrics *loadLyricsFromSYLT(const char *path)
{
    if (!path)
        return nullptr;

    TagLib::FileRef f(path);
    if (!f.file())
        return nullptr;

    auto mpegFile = dynamic_cast<TagLib::MPEG::File *>(f.file());
    if (!mpegFile)
        return nullptr;

    auto id3v2Tag = mpegFile->ID3v2Tag();
    if (!id3v2Tag)
        return nullptr;

    auto frames = id3v2Tag->frameList("SYLT");
    if (frames.isEmpty())
        return nullptr;

    size_t totalLines = 0;
    for (auto frame : frames) {
        auto sylt = dynamic_cast<TagLib::ID3v2::SynchronizedLyricsFrame *>(frame);
        if (!sylt) continue;
        totalLines += sylt->synchedText().size();
    }

    if (totalLines == 0)
        return nullptr;

    Lyrics *lyrics = (Lyrics *)calloc(1, sizeof(Lyrics));
    if (!lyrics)
        return nullptr;

    lyrics->maxLength = 1024;
    lyrics->lines = (LyricsLine *)malloc(sizeof(LyricsLine) * totalLines);
    if (!lyrics->lines) {
        free(lyrics);
        return nullptr;
    }

    lyrics->count = 0;
    for (auto frame : frames) {
        auto sylt = dynamic_cast<TagLib::ID3v2::SynchronizedLyricsFrame *>(frame);
        if (!sylt) continue;

        for (auto lyric : sylt->synchedText()) {
            char *text = strdup(lyric.text.toCString(true));
            if (!text) continue;

            // Trim leading/trailing whitespace
            char *start = text;
            while (isspace((unsigned char)*start)) start++;
            char *end = start + strlen(start);
            while (end > start && isspace((unsigned char)*(end - 1))) *(--end) = '\0';

            lyrics->lines[lyrics->count].timestamp = lyric.time / 1000.0; // ms → s
            lyrics->lines[lyrics->count].text = strdup(start);
            free(text);

            if (!lyrics->lines[lyrics->count].text) {
                freeLyrics(lyrics);
                return nullptr;
            }

            lyrics->count++;
        }
    }

    lyrics->isTimed = 1;
    return lyrics;
}

// LRC Loader
static int loadTimedLyrics(FILE *file, Lyrics *lyrics)
{
    size_t capacity = 64;
    lyrics->lines = (LyricsLine*)malloc(sizeof(LyricsLine) * capacity);
    if (!lyrics->lines)
        return 0;

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
                capacity *= 2;
                LyricsLine *newLines = (LyricsLine*)realloc(lyrics->lines, sizeof(LyricsLine) * capacity);
                if (!newLines)
                    return 0;
                lyrics->lines = newLines;
            }

            char *start = text;
            while (isspace((unsigned char)*start)) start++;
            char *end = start + strlen(start);
            while (end > start && isspace((unsigned char)*(end - 1))) *(--end) = '\0';

            lyrics->lines[lyrics->count].timestamp = min * 60.0 + sec + cs / 100.0;
            lyrics->lines[lyrics->count].text = strdup(start);
            if (!lyrics->lines[lyrics->count].text)
                return 0;

            lyrics->count++;
        }
    }

    lyrics->isTimed = 1;
    return 1;
}

static int loadUntimedLyrics(FILE *file, Lyrics *lyrics)
{
    size_t capacity = 64;
    lyrics->lines = (LyricsLine*)malloc(sizeof(LyricsLine) * capacity);
    if (!lyrics->lines)
        return 0;

    char lineBuffer[1024];
    lyrics->count = 0;

    while (fgets(lineBuffer, sizeof(lineBuffer), file))
    {
        char *newline = strpbrk(lineBuffer, "\r\n");
        if (newline)
            *newline = '\0';

        if (lineBuffer[0] == '\0')
            continue;

        if (lyrics->count == capacity)
        {
            capacity *= 2;
            LyricsLine *newLines = (LyricsLine*)realloc(lyrics->lines, sizeof(LyricsLine) * capacity);
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

static Lyrics *loadLyricsFromLRC(const char *music_path)
{
    char lrcPath[PATH_MAX];
    if (snprintf(lrcPath, sizeof(lrcPath), "%s", music_path) >= (int)sizeof(lrcPath))
        return nullptr;

    char *dot = strrchr(lrcPath, '.');
    if (!dot || dot == lrcPath)
        return nullptr;

    if (snprintf(dot, sizeof(lrcPath) - (dot - lrcPath), ".lrc") >= (int)(sizeof(lrcPath) - (dot - lrcPath)))
        return nullptr;

    FILE *file = fopen(lrcPath, "r");
    if (!file)
        return nullptr;
Lyrics *lyrics = (Lyrics*)calloc(1, sizeof(Lyrics));

    if (!lyrics) {
        fclose(file);
        return nullptr;
    }

    lyrics->maxLength = 1024;

    // Detect if there are timestamps
    char lineBuffer[1024];
    int foundTimestamp = 0;
    while (fgets(lineBuffer, sizeof(lineBuffer), file))
    {
        if (lineBuffer[0] == '[' && isdigit((unsigned char)lineBuffer[1]))
        {
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

// General Loader
Lyrics *loadLyrics(const char *path)
{
    if (!path)
        return nullptr;

    // 1. Try LRC file first (priority)
    Lyrics *lyrics = loadLyricsFromLRC(path);
    if (lyrics)
        return lyrics;

    // 2. Fallback to ID3 SYLT
    return loadLyricsFromSYLT(path);
}

// Free & Access
void freeLyrics(Lyrics *lyrics)
{
    if (!lyrics) return;
    for (size_t i = 0; i < lyrics->count; i++)
        free(lyrics->lines[i].text);
    free(lyrics->lines);
    free(lyrics);
}


