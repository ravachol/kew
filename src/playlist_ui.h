#include "playlist.h"
#include "songloader.h"
#include "term.h"
#include "utils.h"

int displayPlaylist(PlayList *list, int maxListSize, int indent, int chosenSong, int *chosenNodeId, bool reset);
