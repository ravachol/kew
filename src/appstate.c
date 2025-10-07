#include "appstate.h"

AppState appState;

// The (sometimes shuffled) sequence of songs that will be played
PlayList playlist = {NULL, NULL, 0, PTHREAD_MUTEX_INITIALIZER};

// The playlist unshuffled as it appears in playlist view
PlayList *unshuffledPlaylist = NULL;

// The playlist from kew favorites .m3u
PlayList *favoritesPlaylist = NULL;
