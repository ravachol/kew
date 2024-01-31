
#ifndef PLAYEROPS_H
#define PLAYEROPS_H

#include <stdbool.h>
#include "songloader.h"

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
extern LoadingThreadData loadingdata;
extern double elapsedSeconds;
extern double pauseSeconds;
extern double totalPauseSeconds;
extern struct timespec pause_time;
extern volatile bool loadedNextSong;
extern bool playlistNeedsUpdate;
extern bool nextSongNeedsRebuilding;
extern bool enqueuedNeedsUpdate;
extern bool waitingForPlaylist;
extern bool waitingForNext;
extern bool usingSongDataA;
extern Node *nextSong;
extern int lastPlayedId;
extern bool playingMainPlaylist;
extern bool songHasErrors;
extern bool doQuit;
extern bool loadingFailed;
extern volatile bool clearingErrors;
extern volatile bool songLoading;
extern struct timespec start_time;
extern bool skipping;
extern bool skipPrev;
extern Node *tryNextSong;
extern struct timespec lastInputTime;
extern struct timespec lastPlaylistChangeTime;

extern UserData userData;

SongData *getCurrentSongData(void);

void rebuildAndUpdatePlaylist();

Node *getNextSong();

void handleRemove();

void enqueueSong(FileSystemEntry *child);

void dequeueSong(FileSystemEntry *child);

void dequeueChildren(FileSystemEntry *parent);

void enqueueChildren(FileSystemEntry *child);

bool markAsDequeued(FileSystemEntry *root, char *path);

void enqueueSongs();

void rebuildNextSong();

void updateLastSongSwitchTime(void);

void updateLastPlaylistChangeTime();

void updateLastInputTime(void);

void playbackPause(double *totalPauseSeconds, double *pauseSeconds, struct timespec *pause_time);

void playbackPlay(double *totalPauseSeconds, double *pauseSeconds, struct timespec *pause_time);

void togglePause(double *totalPauseSeconds, double *pauseSeconds, struct timespec *pause_time);

void toggleRepeat(void);

void toggleShuffle(void);

void addToPlaylist(void);

void toggleBlocks(void);

void toggleColors(void);

void toggleCovers(void);

void toggleVisualizer(void);

void quit(void);

void calcElapsedTime(void);

Node *getSongByNumber(PlayList *playlist, int songNumber);

void skipToNextSong(void);

void skipToPrevSong(void);

void skipToSong(int id);

void seekForward(void);

void seekBack(void);

void skipToNumberedSong(int songNumber);

void skipToLastSong(void);

void loadSong(Node *song, LoadingThreadData *loadingdata);

void loadNext(LoadingThreadData *loadingdata);

void loadFirst(Node *song);

void flushSeek(void);

Node *findSelectedEntryById(PlayList *playlist, int id);

void emitSeekedSignal(double newPositionSeconds);

#endif
