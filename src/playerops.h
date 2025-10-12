
#ifndef PLAYEROPS_H
#define PLAYEROPS_H


#include "appstate.h"
#include "playlist.h"
#include <gio/gio.h>

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

struct timespec getPauseTime(void);

bool hasErrorsSong(void);

void tryLoadNext(AppState *state);

void setGMainContext(GMainContext *val);

void  *getGMainContext(void);

bool shouldRefreshPlayer(void);

Node *chooseNextSong(void);

double getElapsedSeconds(void);

void handleRemove(AppState *state);

void handleSkipFromStopped(void);

FileSystemEntry *enqueueSongs(FileSystemEntry *entry, AppState *state);

void toggleRepeat(AppState *state);

void toggleNotifications(UISettings *ui, AppSettings *settings);

void toggleShuffle(AppState *state);

void toggleAscii(AppSettings *settings, UISettings *ui);

void cycleColorMode(AppState *state);

void cycleThemes(AppState *state, AppSettings *settings);

void toggleVisualizer(AppSettings *settings, UISettings *ui);

void toggleShowLyricsPage(AppState *state);

void quit(void);

int loadTheme(AppState *appState, AppSettings *settings, const char *themeName,
              bool isAnsiTheme);

void calcElapsedTime(void);

Node *getSongByNumber(PlayList *playlist, int songNumber);

void skipToSong(int id, bool startPlaying, AppState *state);

void flushSeek(void);

Node *findSelectedEntryById(PlayList *playlist, int id);

void emitSeekedSignal(double newPositionSeconds);

void rebuildNextSong(Node *song, AppState *state);

void updateLibrary(AppState *state, char *path);

void askIfCacheLibrary(UISettings *ui);

void unloadSongA(AppState *state);

void unloadSongB(AppState *state);

void unloadPreviousSong(AppState *state);

void createLibrary(AppSettings *settings, AppState *state);

void resetClock(void);

void loadNextSong(AppState *state);

void setCurrentSongToNext(void);

void finishLoading(UIState *uis);

bool setPosition(gint64 newPosition);

bool seekPosition(gint64 offset);

void silentSwitchToNext(bool loadSong, AppState *state);

void reshufflePlaylist(void);

bool determineCurrentSongData(SongData **currentSongData);

void updateLibraryIfChangedDetected(AppState *state);

double getCurrentSongDuration(void);

void dequeueAllExceptPlayingSong(AppState *state);

bool isContainedWithin(FileSystemEntry *entry, FileSystemEntry *containingEntry);

void addToFavoritesPlaylist(void);

#endif
