#ifndef PLAYLIST_OPS_H
#define PLAYLIST_OPS_H

#include "playlist.h"
#include "appstate.h"

Node *chooseNextSong(void);
Node *getSongByNumber(PlayList *playlist, int songNumber);
Node *findSelectedEntryById(PlayList *playlist, int id);
Node *findSelectedEntry(PlayList *playlist, int row);
void reshufflePlaylist(void);
void repeatList(AppState *state);
void moveSongUp(AppState *state, int *chosenRow);
void moveSongDown(AppState *state, int *chosenRow);
void removeCurrentlyPlayingSong(AppState *state);
void skipToNumberedSong(int songNumber, AppState *state);
void skipToLastSong(AppState *state);
void skipToPrevSong(AppState *state);
void skipToNextSong(AppState *state);

#endif
