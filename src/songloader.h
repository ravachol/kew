#include <sys/wait.h>
#include <FreeImage.h>
#include "metadata.h"
#include "file.h"
#include "cache.h"
#include "chafafunc.h"
#include "albumart.h"
#include "soundgapless.h"

#ifndef KEYVALUEPAIR_STRUCT
#define KEYVALUEPAIR_STRUCT

typedef struct
{
    char *key;
    char *value;
} KeyValuePair;

#endif


#ifndef TAGSETTINGS_STRUCT
#define TAGSETTINGS_STRUCT

typedef struct
{
    char title[256];
    char artist[256];
    char album_artist[256];
    char album[256];
    char date[256];
} TagSettings;

#endif

#ifndef SONGDATA_STRUCT
#define SONGDATA_STRUCT

typedef struct
{
	char filePath[MAXPATHLEN];
	char coverArtPath[MAXPATHLEN];
	char pcmFilePath[MAXPATHLEN];
	unsigned char *red;
	unsigned char *green;
	unsigned char *blue;
	TagSettings *metadata;
	FIBITMAP *cover;
    double *duration;
    char* pcmFile;
    long pcmFileSize;

} SongData;

#endif

SongData* loadSongData(char *filePath);
void unloadSongData(SongData *songdata);
