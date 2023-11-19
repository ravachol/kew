#ifndef SOUNDGAPLESS_H
#define SOUNDGAPLESS_H
#include <miniaudio.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdatomic.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "file.h"
#include "songloader.h"
#include "soundcommon.h"
#include "soundbuiltin.h"
#include "soundpcm.h"

void setDecoders(bool usingA, char *filePath);

void createAudioDevice(UserData *userData);

void switchAudioImplementation();

void cleanupAudioContext();

#endif
