/**
 * @file artists.h
 * @brief Provides artists database functions.
 *
 */

#ifndef ARTISTS_H
#define ARTISTS_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define DB_MAGIC 0x4B455744 /* "KEWD" */

typedef struct {
    uint32_t name_offset;
    uint32_t value_offset;
} ArtistRecord;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t count;
    uint32_t strings_offset;
} Header;

typedef struct {
#ifdef _WIN32
    HANDLE file;
    HANDLE mapping_handle;
#endif
    int fd;
    size_t size;
    void *mapping;

    Header *header;
    ArtistRecord *records;
    char *strings;
} ArtistDb;

int db_open(ArtistDb *db, const char *path);

void db_close(ArtistDb *db);

const ArtistRecord *db_find(ArtistDb *db, const char *name);

const char *db_get_value(ArtistDb *db, const ArtistRecord *r);

#endif
