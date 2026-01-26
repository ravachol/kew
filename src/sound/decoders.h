/**
 * @file sound.[h]
 * @brief Decoders.
 *
 * Decoder related functions.
 */

#ifndef DECODERS_H
#define DECODERS_H

#include "common/appstate.h"

#include "audiotypes.h"
#include "webm.h"

#include <miniaudio.h>
#include <miniaudio_libopus.h>
#include <miniaudio_libvorbis.h>
#include <stdbool.h>

void reset_all_decoders(void);
bool has_builtin_decoder(const char *file_path);
void switch_decoder(void);

int prepare_next_decoder(const char *filepath);
int prepare_next_opus_decoder(const char *filepath);
int prepare_next_vorbis_decoder(const char *filepath);
int prepare_next_m4a_decoder(SongData *song_data);
int prepare_next_webm_decoder(SongData *song_data);

ma_decoder *get_first_decoder(void);
ma_decoder *get_current_builtin_decoder(void);

ma_libopus *get_current_opus_decoder(void);
ma_libopus *get_first_opus_decoder(void);

ma_libvorbis *get_current_vorbis_decoder(void);
ma_libvorbis *get_first_vorbis_decoder(void);

ma_webm *get_current_webm_decoder(void);
ma_webm *get_first_webm_decoder(void);

void clear_decoder_chain();

#ifdef USE_FAAD
m4a_decoder *get_current_m4a_decoder(void);
m4a_decoder *get_first_m4a_decoder(void);
#endif

#endif
