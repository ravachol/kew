#ifndef DECODERS_H
#define DECODERS_H

#include "common/appstate.h"

#include "audiotypes.h"
#include "webm.h"

#include <miniaudio.h>
#include <miniaudio_libopus.h>
#include <miniaudio_libvorbis.h>
#include <stdbool.h>

void resetAllDecoders(void);
bool hasBuiltinDecoder(const char *filePath);
void switchDecoder(void);

int prepareNextDecoder(const char *filepath);
int prepareNextOpusDecoder(const char *filepath);
int prepareNextVorbisDecoder(const char *filepath);
int prepareNextM4aDecoder(SongData *songData);
int prepareNextWebmDecoder(SongData *songData);

ma_decoder *getFirstDecoder(void);
ma_decoder *getCurrentBuiltinDecoder(void);

ma_libopus *getCurrentOpusDecoder(void);
ma_libopus *getFirstOpusDecoder(void);

ma_libvorbis *getCurrentVorbisDecoder(void);
ma_libvorbis *getFirstVorbisDecoder(void);

ma_webm *getCurrentWebmDecoder(void);
ma_webm *getFirstWebmDecoder(void);

void clearDecoderChain();

#ifdef USE_FAAD
m4a_decoder *getCurrentM4aDecoder(void);
m4a_decoder *getFirstM4aDecoder(void);
#endif

#endif
