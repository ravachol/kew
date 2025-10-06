
#ifndef PLAYEROPS_H
#define PLAYEROPS_H


#include "appstate.h"
#include "playlist.h"
#include "sound.h"
#include "soundcommon.h"

#ifdef USE_FAAD
#include "m4a.h"
#endif
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
} LoadingThreadData;

extern GDBusConnection *connection;
extern GMainContext *global_main_context;
extern LoadingThreadData loadingdata;
extern struct timespec start_time;
extern struct timespec pause_time;
extern volatile bool loadedNextSong;
extern bool nextSongNeedsRebuilding;
extern bool waitingForPlaylist;
extern bool waitingForNext;
extern bool usingSongDataA;
extern Node *nextSong;
extern Node *songToStartFrom;
extern int lastPlayedId;
extern bool songHasErrors;
extern volatile bool clearingErrors;
extern volatile bool songLoading;
extern bool skipping;
extern bool skipOutOfOrder;
extern Node *tryNextSong;
extern bool skipFromStopped;
extern UserData userData;

SongData *getCurrentSongData(void);

Node *getNextSong(void);

void handleRemove(void);

FileSystemEntry *enqueueSongs(FileSystemEntry *entry, UIState *uis);

void resetStartTime(void);

void playbackPause(struct timespec *pause_time);

void playbackPlay(double *totalPauseSeconds, double *pauseSeconds);

void togglePause(double *totalPauseSeconds, double *pauseSeconds, struct timespec *pause_time);

void stop(void);

void toggleRepeat(UISettings *ui);

void toggleShuffle(UISettings *ui);

void toggleAscii(AppSettings *settings, UISettings *ui);

void toggleColors(AppSettings *settings, UISettings *ui);

void toggleVisualizer(AppSettings *settings, UISettings *ui);

void quit(void);

void calcElapsedTime(void);

Node *getSongByNumber(PlayList *playlist, int songNumber);

void skipToNextSong(AppState *state);

void skipToPrevSong(AppState *state);

void skipToSong(int id, bool startPlaying);

void seekForward(UIState *uis);

void seekBack(UIState *uis);

void skipToNumberedSong(int songNumber);

void skipToLastSong(void);

void loadSong(Node *song, LoadingThreadData *loadingdata);

int loadFirst(Node *song, AppState *state);

void flushSeek(void);

Node *findSelectedEntryById(PlayList *playlist, int id);

void emitSeekedSignal(double newPositionSeconds);

void rebuildNextSong(Node *song);

void updateLibrary(char *path);

void askIfCacheLibrary(UISettings *ui);

void unloadSongA(AppState *state);

void unloadSongB(AppState *state);

void unloadPreviousSong(AppState *state);

void createLibrary(AppSettings *settings, AppState *state);

void resetClock(void);

void loadNextSong(void);

void setCurrentSongToNext(void);

void finishLoading(void);

void resetTimeCount(void);

bool setPosition(gint64 newPosition);

bool seekPosition(gint64 offset);

void silentSwitchToNext(bool loadSong, AppState *state);

void reshufflePlaylist(void);

bool determineCurrentSongData(SongData **currentSongData);

void updateLibraryIfChangedDetected(void);

double getCurrentSongDuration(void);

void updatePlaylistToPlayingSong(void);

void moveSongUp();

void moveSongDown();

void play(Node *song);

void repeatList();

void skipToBegginningOfSong(void);

void sortLibrary(void);

void markListAsEnqueued(FileSystemEntry *root, PlayList *playlist);

bool isContainedWithin(FileSystemEntry *entry, FileSystemEntry *containingEntry);

void addToFavoritesPlaylist(void);

void autostartIfStopped(FileSystemEntry *firstEnqueuedEntry);

#endif
