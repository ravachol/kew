#ifndef SOUND_RADIO_H
#define SOUND_RADIO_H

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

int internetRadioSearch(const char *searchTerm, void (*callback)(const char *, const char *, const char *, const char *, const int, const int));

int playRadioStation(const RadioSearchResult *station);

void stopRadio(void);

bool isRadioPlaying(void);

RadioSearchResult *getCurrentPlayingRadioStation(void);

void setCurrentlyPlayingRadioStation(const RadioSearchResult * result);

void freeCurrentlyPlayingRadioStation(void);

#endif
