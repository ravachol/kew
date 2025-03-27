#ifndef SOUND_RADIO_H
#define SOUND_RADIO_H

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>
#include <pthread.h>
#include "soundcommon.h"
#include "common.h"

typedef struct RadioSearchResult
{
    char name[256];
    char url_resolved[2048];
    char country[64];
    char codec[32];
    int bitrate;
    int votes;
} RadioSearchResult;

#define STREAM_BUFFER_SIZE (256 * 1024)

typedef struct
{
        unsigned char buffer[STREAM_BUFFER_SIZE];
        size_t write_pos;
        size_t read_pos;
        int eof;
        pthread_mutex_t mutex;
        pthread_cond_t cond;
        time_t last_data_time;
        bool stale;
} stream_buffer;

typedef struct
{
        stream_buffer buf;
        CURL *curl;
        pthread_t curl_thread;
        ma_decoder decoder;
} RadioPlayerContext;

extern RadioPlayerContext radioContext;

int internetRadioSearch(const char *searchTerm, void (*callback)(const char *, const char *, const char *, const char *, const int, const int));

int playRadioStation(const RadioSearchResult *station);

void stopRadio(void);

void reconnectRadioIfNeeded();

bool isRadioPlaying(void);

RadioSearchResult *getCurrentPlayingRadioStation(void);

void setCurrentlyPlayingRadioStation(const RadioSearchResult * result);

void freeCurrentlyPlayingRadioStation(void);

#endif
