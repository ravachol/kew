/**
 * @file playlist_ops.[h]
 * @brief Playlist management operations.
 *
 * Implements functionality for adding, removing, reordering,
 * shuffling, and retrieving songs within playlists.
 * Coordinates with the playback system for next/previous transitions.
 */

#ifndef PLAYLIST_OPS_H
#define PLAYLIST_OPS_H

#include "common/appstate.h"

#include "data/playlist.h"

Node *chooseNextSong(void);
Node *getSongByNumber(PlayList *playlist, int songNumber);
Node *findSelectedEntryById(PlayList *playlist, int id);
Node *findSelectedEntry(PlayList *playlist, int row);
void repeatList(AppState *state);
void moveSongUp(AppState *state, int *chosenRow);
void moveSongDown(AppState *state, int *chosenRow);
void removeCurrentlyPlayingSong(AppState *state);
void skipToNumberedSong(int songNumber, AppState *state);
void skipToLastSong(AppState *state);
void skipToPrevSong(AppState *state);
void skipToNextSong(AppState *state);
void setCurrentSongToNext(void);
void rebuildNextSong(Node *song, AppState *state);
void dequeueAllExceptPlayingSong(AppState *state);
void handleRemove(AppState *state, int chosenRow);
void addToFavoritesPlaylist(void);
void reshufflePlaylist(void);

#endif
