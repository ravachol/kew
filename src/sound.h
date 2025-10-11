#ifndef SOUND_H
#define SOUND_H

#include "appstate.h"
#include <fcntl.h>
#include <miniaudio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "songloader.h"

UserData *getUserData(void);

bool isContextInitialized(void);

int createAudioDevice(AppState *state);

int switchAudioImplementation(AppState *state);

void resumePlayback(AppState *state);

void cleanupAudioContext(void);

#endif
