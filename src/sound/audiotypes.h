/**
 * @file audiotypes.h
 * @brief Decoders.
 *
 * Supported Audio types. Built-in are miniaudios supported audio types flac, mp3 and wav.
 */

#ifndef AUDIOTYPES_H
#define AUDIOTYPES_H

#include "loader/songdatatype.h"

#include <miniaudio.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
        SOUND_STATE_STOPPED = 0,
        SOUND_STATE_PLAYING,
        SOUND_STATE_PAUSED
} sound_playback_state_t;

typedef enum {
        SOUND_STATE_REPEAT_OFF = 0,
        SOUND_STATE_REPEAT,
        SOUND_STATE_REPEAT_LIST
} sound_playback_repeat_state_t;

struct sound_system {
        void *decoder;
        ma_device *device;

        bool decode_thread_active; // FIXME should be atomic
        bool replay_gain_check_track_first;

        ma_uint32 channels;
        ma_uint32 sample_rate;
        ma_format format;

        ma_uint64 currentPCMFrame;
        ma_uint64 totalFrames;

        ma_uint32 avg_bit_rate;

        SongData *songdataA;
        SongData *songdataB;

        ma_uint32 chunk_frames;
        pthread_t decode_thread;

#ifndef __cplusplus
        atomic_long track_frames_sent;
        atomic_long track_end_frame;
        atomic_bool buffer_ready;
        atomic_bool end_of_list_reached;
        atomic_bool decode_thread_running;
        atomic_bool decode_finished;
        atomic_bool pending_switch;
        atomic_bool switch_files;
        atomic_bool using_song_slot_A;
#endif

        float volume;
        sound_playback_state_t state;
};

enum decoder_type {
        PCM,
        BUILTIN,
        VORBIS,
        OPUS,
        M4A,
        WEBM,
        NONE
};

typedef enum {
        k_unknown = 0,
        k_aac = 1,
        k_rawAAC = 2, // Raw aac (.aac file) decoding is included here for convenience although they are not .m4a files
        k_ALAC = 3,
        k_FLAC = 4
} k_m4adec_filetype;

struct m4a_decoder;
typedef struct m4a_decoder ma_m4a;

#endif
