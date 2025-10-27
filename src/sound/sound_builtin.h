/**
 * @file sound_builtin.[h]
 * @brief Built-in audio backend implementation.
 *
 * Implements a simple internal audio output backend used when no
 * external library is available. Useful for portability and testing.
 */

#ifndef sound_builtin_H
#define sound_builtin_H

#include <miniaudio.h>

extern ma_data_source_vtable builtin_file_data_source_vtable;
void builtin_read_pcm_frames(ma_data_source *p_data_source, void *p_frames_out, ma_uint64 frame_count, ma_uint64 *p_frames_read);
void builtin_on_audio_frames(ma_device *p_device, void *p_frames_out, const void *p_frames_in, ma_uint32 frame_count);

#endif
