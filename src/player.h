#include <stdbool.h>
#include <stdio.h>
#include "albumart.h"
#include "term.h"
#include "printfunc.h"
#include "visuals.h"
#include "playlist.h"

extern bool coverEnabled;
extern bool coverBlocks;
extern bool visualizationEnabled;
extern int visualizationHeight;
extern bool refresh;

int printPlayer(const char *songFilepath, const char *tagsFilePath, double elapsedSeconds, double songDurationSeconds, PlayList *playlist);