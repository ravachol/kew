/**
 * @file playlist_ops.h
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

Node *getSongByNumber(PlayList *playlist, int songNumber);
Node *findSelectedEntryById(PlayList *playlist, int id);
Node *findSelectedEntry(PlayList *playlist, int row);
Node *determineNextSong(PlayList *playlist);
void rebuildNextSong(Node *song);
void repeatList(void);
void moveSongUp(int *chosenRow);
void moveSongDown(int *chosenRow);
void handleRemove(int chosenRow);
void skipToNumberedSong(int songNumber);
void removeCurrentlyPlayingSong(void);
void skipToLastSong(void);
void skipToPrevSong(void);
void skipToNextSong(void);
void setCurrentSongToNext(void);
void dequeueAllExceptPlayingSong(void);
void addToFavoritesPlaylist(void);
void reshufflePlaylist(void);
void handleSkipOutOfOrder(void);
void playlistPlay(PlayList *playlist);
void clearAndPlay(Node *song);
bool playPreProcessing();
void playPostProcessing(bool wasEndOfList);
void playFavoritesPlaylist(void);
void playAll(void);
void playAllAlbums(void);

#endif
