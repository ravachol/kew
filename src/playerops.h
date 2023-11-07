
#ifndef PLAYEROPS_H
#define PLAYEROPS_H

#include <stdbool.h>
#include "songloader.h"

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

typedef struct
{
        char filePath[MAXPATHLEN];
        SongData *songdataA;
        SongData *songdataB;
        bool loadA;
        pthread_mutex_t mutex;
} LoadingThreadData;

extern GDBusConnection *connection;
extern LoadingThreadData loadingdata;
extern double elapsedSeconds;
extern double pauseSeconds;
extern double totalPauseSeconds;
extern struct timespec pause_time;
extern volatile bool loadedNextSong;
extern bool usingSongDataA;
extern Node *nextSong;
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

extern UserData userData;

SongData *getCurrentSongData();

void updateLastSongSwitchTime();

void updateLastInputTime();

void emitMetadataChanged(const gchar *title, const gchar *artist, const gchar *album, const gchar *coverArtPath, const gchar *trackId);

void playbackPause(double *totalPauseSeconds, double *pauseSeconds, struct timespec *pause_time);

void playbackPlay(double *totalPauseSeconds, double *pauseSeconds, struct timespec *pause_time);

void togglePause(double *totalPauseSeconds, double *pauseSeconds, struct timespec *pause_time);

void toggleRepeat();

void toggleShuffle();

void addToPlaylist();

void toggleBlocks();

void toggleColors();

void toggleCovers();

void toggleVisualizer();

void quit();

void calcElapsedTime();

Node *getSongByNumber(int songNumber);

void skipToNextSong();

void skipToPrevSong();

void seekForward();

void seekBack();

void skipToNumberedSong(int songNumber);

void skipToLastSong();

void loadSong(Node *song, LoadingThreadData *loadingdata);

void loadNext(LoadingThreadData *loadingdata);

void loadFirst(Node *song);

void flushSeek();

#endif