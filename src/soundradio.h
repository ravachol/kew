#ifndef SOUND_RADIO_H
#define SOUND_RADIO_H

#include <curl/curl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>

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
        bool isRunning;
} RadioPlayerContext;

extern RadioPlayerContext radioContext;

int internetRadioSearch(const char *searchTerm, void (*callback)(const char *, const char *, const char *, const char *, const int, const int));

int playRadioStation(const RadioSearchResult *station);

int stopRadio(void);

void reconnectRadioIfNeeded();

bool isRadioPlaying(void);

bool IsActiveRadio(void);

RadioSearchResult *getCurrentPlayingRadioStation(void);

void setCurrentlyPlayingRadioStation(const RadioSearchResult * result);

void freeCurrentlyPlayingRadioStation(void);

void initRadioMutexes(void);

void destroyRadioMutexes(void);

void stopCurrentRadioSearchThread(void);

#endif
