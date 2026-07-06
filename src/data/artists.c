#include "artists.h"
#include <ctype.h>

static ArtistDb *g_db;

int db_open(ArtistDb *db, const char *path)
{
    memset(db, 0, sizeof(*db));

    db->fd = -1;

    db->fd = open(path, O_RDONLY);
    if (db->fd < 0)
        return -1;

    struct stat st;
    if (fstat(db->fd, &st) != 0)
        return -1;

    db->size = st.st_size;

    db->mapping = mmap(NULL,
                       db->size,
                       PROT_READ,
                       MAP_PRIVATE,
                       db->fd,
                       0);

    if (db->mapping == MAP_FAILED)
        return -1;

    db->header = (Header *)db->mapping;

    if (db->header->magic != DB_MAGIC)
        return -1;

    db->records = (ArtistRecord *)((char *)db->mapping + sizeof(Header));

    db->strings =
        (char *)db->mapping + db->header->strings_offset;

    return 0;
}

void db_close(ArtistDb *db)
{
    if (db->mapping)
        munmap(db->mapping, db->size);

    if (db->fd >= 0)
        close(db->fd);
}

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
