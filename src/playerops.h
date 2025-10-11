
#ifndef PLAYEROPS_H
#define PLAYEROPS_H


#include "appstate.h"
#include "playlist.h"
#include "sound.h"
#include <gio/gio.h>

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

typedef struct
{
        char filePath[MAXPATHLEN];
        SongData *songdataA;
        SongData *songdataB;
        bool loadA;
        bool loadingFirstDecoder;
        pthread_mutex_t mutex;
        AppState *state;
} LoadingThreadData;

LoadingThreadData *getLoadingData();

bool isSkipOutOfOrder(void);

void setSkipOutOfOrder(bool val);

bool isSongLoading(void);

void setSongLoading(bool val);

struct timespec getPauseTime(void);

bool hasErrorsSong(void);

int getLastPlayedId(void);

void setLastPlayedId(int id);

void tryLoadNext(AppState *state);

void setGMainContext(GMainContext *val);

void  *getGMainContext(void);

GDBusConnection *getGDBusConnection(void);

void setGDBusConnection(GDBusConnection *val);

bool shouldRefreshPlayer(void);

void setIsUsingSongDataA(bool val);

bool isUsingSongDataA(void);

void setNextSongNeedsRebuilding(bool val);

bool getNextSongNeedsRebuilding(void);

SongData *getCurrentSongData(void);

bool hasSkippedFromStopped(void);

Node *chooseNextSong(void);

double getElapsedSeconds(void);

void handleRemove(AppState *state);

void handleSkipFromStopped(void);

FileSystemEntry *enqueueSongs(FileSystemEntry *entry, AppState *state);

void playbackPause(AppState *state, struct timespec *pause_time);

void playbackPlay(AppState *state);

void togglePause(AppState *state);

void stop(AppState *state);

void toggleRepeat(AppState *state);

void toggleNotifications(UISettings *ui, AppSettings *settings);

void toggleShuffle(AppState *state);

void toggleAscii(AppSettings *settings, UISettings *ui);

void cycleColorMode(AppState *state);

void cycleThemes(AppState *state, AppSettings *settings);

void toggleVisualizer(AppSettings *settings, UISettings *ui);

void quit(void);

int loadTheme(AppState *appState, AppSettings *settings, const char *themeName,
              bool isAnsiTheme);

void calcElapsedTime(void);

Node *getSongByNumber(PlayList *playlist, int songNumber);

void skipToNextSong(AppState *state);

void skipToPrevSong(AppState *state);

void skipToNumberedSong(int songNumber, AppState *state);

void skipToLastSong(AppState *state);

void skipToSong(int id, bool startPlaying, AppState *state);

void seekForward(UIState *uis);

void seekBack(UIState *uis);

void loadSong(Node *song, LoadingThreadData *loadingdata, UIState *uis);

int loadFirst(Node *song, AppState *state);

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

void updatePlaylistToPlayingSong(AppState *state);

void moveSongUp(AppState *state);

void moveSongDown(AppState *state);

void play(Node *node, AppState *state);

void repeatList(AppState *state);

void skipToBegginningOfSong(void);

void sortLibrary(void);

void markListAsEnqueued(FileSystemEntry *root, PlayList *playlist);

bool isContainedWithin(FileSystemEntry *entry, FileSystemEntry *containingEntry);

void addToFavoritesPlaylist(void);

void autostartIfStopped(FileSystemEntry *firstEnqueuedEntry, UIState *uis);

#endif
