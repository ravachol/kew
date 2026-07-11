#include "artists.h"
#include <ctype.h>

static ArtistDb *g_db;

#ifdef _WIN32

int db_open(ArtistDb *db, const char *path)
{
    memset(db, 0, sizeof(*db));

    db->file = INVALID_HANDLE_VALUE;

    db->file = CreateFileA(path,
                           GENERIC_READ,
                           FILE_SHARE_READ,
                           NULL,
                           OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL,
                           NULL);

    if (db->file == INVALID_HANDLE_VALUE)
        goto fail;

    LARGE_INTEGER size;

    if (!GetFileSizeEx(db->file, &size))
        goto fail;

    db->size = (size_t)size.QuadPart;

    if (db->size < sizeof(Header))
        goto fail;

    db->mapping_handle = CreateFileMappingA(db->file,
                                            NULL,
                                            PAGE_READONLY,
                                            0,
                                            0,
                                            NULL);

    if (!db->mapping_handle)
        goto fail;

    db->mapping = MapViewOfFile(db->mapping_handle,
                                FILE_MAP_READ,
                                0,
                                0,
                                0);

    if (!db->mapping)
        goto fail;

    db->header = (Header *)db->mapping;

    if (db->header->magic != DB_MAGIC)
        goto fail;

    if (db->header->strings_offset >= db->size)
        goto fail;

    db->records = (ArtistRecord *)((char *)db->mapping + sizeof(Header));
    db->strings = (char *)db->mapping + db->header->strings_offset;

    return 0;

fail:
    db_close(db);
    return -1;
}

void db_close(ArtistDb *db)
{
    if (db->mapping) {
        UnmapViewOfFile(db->mapping);
        db->mapping = NULL;
    }

    if (db->mapping_handle) {
        CloseHandle(db->mapping_handle);
        db->mapping_handle = NULL;
    }

    if (db->file != INVALID_HANDLE_VALUE) {
        CloseHandle(db->file);
        db->file = INVALID_HANDLE_VALUE;
    }
}

#else

int db_open(ArtistDb *db, const char *path)
{
    memset(db, 0, sizeof(*db));

    db->fd = -1;

    db->fd = open(path, O_RDONLY);
    if (db->fd < 0)
        goto fail;

    struct stat st;

    if (fstat(db->fd, &st) != 0)
        goto fail;

    db->size = st.st_size;

    if (db->size < sizeof(Header))
        goto fail;

    db->mapping = mmap(NULL,
                       db->size,
                       PROT_READ,
                       MAP_PRIVATE,
                       db->fd,
                       0);

    if (db->mapping == MAP_FAILED) {
        db->mapping = NULL;
        goto fail;
    }

    db->header = (Header *)db->mapping;

    if (db->header->magic != DB_MAGIC)
        goto fail;

    if (db->header->strings_offset >= db->size)
        goto fail;

    db->records = (ArtistRecord *)((char *)db->mapping + sizeof(Header));
    db->strings = (char *)db->mapping + db->header->strings_offset;

    return 0;

fail:
    db_close(db);
    return -1;
}

void db_close(ArtistDb *db)
{
    if (db->mapping) {
        munmap(db->mapping, db->size);
        db->mapping = NULL;
    }

    if (db->fd >= 0) {
        close(db->fd);
        db->fd = -1;
    }
}

#endif

static int ci_strcmp(const char *a, const char *b)
{
    while (*a && *b) {
        unsigned char ca = (unsigned char)*a;
        unsigned char cb = (unsigned char)*b;

        ca = (unsigned char)tolower(ca);
        cb = (unsigned char)tolower(cb);

        if (ca != cb)
            return (int)ca - (int)cb;

        ++a;
        ++b;
    }

    return (int)tolower((unsigned char)*a)
         - (int)tolower((unsigned char)*b);
}

static int db_cmp(const void *key, const void *elem)
{
    const char *name = key;
    const ArtistRecord *r = elem;

    return ci_strcmp(name, g_db->strings + r->name_offset);
}

const ArtistRecord *db_find(ArtistDb *db, const char *name)
{
    g_db = db;

    return bsearch(name,
                   db->records,
                   db->header->count,
                   sizeof(ArtistRecord),
                   db_cmp);
}

const char *db_get_value(ArtistDb *db, const ArtistRecord *r)
{
    return db->strings + r->value_offset;
}
