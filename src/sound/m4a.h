/**
 * @file m4a.h
 * @brief M4A/AAC decoder interface.
 *
 * Provides decoding support for M4A and AAC-encoded audio files,
 * wrapping platform or library-specific decoding routines.
 */

#ifndef M4A_H
#define M4A_H

#ifdef __cplusplus
extern "C" {
#endif
#ifdef USE_FAAD

#include "audiotypes.h"

#include "../include/libmp4/include/libmp4.h"
#include "neaacdec.h"
#include <alac/alac.h>
#include <miniaudio.h>

#if defined(MINIAUDIO_IMPLEMENTATION) || defined(MA_IMPLEMENTATION)
#include "common/common.h"
#include "utils/k_log.h"
#endif

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAX_CHANNELS 2
#define MAX_SAMPLES 8192 // Maximum expected frame size
#define MAX_SAMPLE_SIZE 4

typedef struct m4a_decoder {
        ma_data_source_base ds; // The m4a decoder can be used independently as a data source.
        ma_read_proc onRead;
        ma_seek_proc onSeek;
        ma_tell_proc onTell;
        void *pReadSeekTellUserData;
        ma_format format;
        FILE *mf;

        // faad2 related fields...
        NeAACDecHandle hDecoder;
        NeAACDecFrameInfo frameInfo;
        unsigned char *buffer;
        unsigned int buffer_size;
        ma_uint32 sampleSize;
        int bit_depth;
        ma_uint32 sample_rate;
        ma_uint32 channels;
        ma_uint32 avg_bit_rate;
        double duration;
        unsigned long totalFrames;

        k_m4adec_filetype file_type;

        ma_int64 file_size;

        // libmp4 fields...
        struct mp4_demux *mp4;
        struct mp4_track_info track;
        unsigned int audio_track_id;

        // alac fields...
        alac_file *alac;
        uint8_t *alacScratch; // Reusable decode output buffer
        size_t alacScratchSize;

        int32_t audio_track_index;
        uint32_t current_sample; // Index of the next MP4 track sample to decode. This is not a PCM frame counter.
        uint32_t total_samples;

        // Raw ADTS frame index, built once so seeking does not rescan the file.
        uint64_t *adts_frame_offsets;
        uint64_t *adts_pcm_offsets;
        uint32_t adts_frame_count;
        uint64_t adts_total_pcm_frames;

        uint8_t leftoverBuffer[MAX_SAMPLES *
                               MAX_CHANNELS *
                               MAX_SAMPLE_SIZE];

        ma_uint64 leftoverSampleCount;

        // For m4a_decoder_init_file
        FILE *file;

        ma_uint64 cursor;
} m4a_decoder;

#define FOUR_CHAR_INT(a, b, c, d) (((uint32_t)(a) << 24) | ((b) << 16) | ((c) << 8) | (d))

/**
 * @brief Initializes an M4A decoder using custom I/O callbacks.
 *
 * Sets up an M4A decoding context using user-provided read, seek, and
 * optional tell callbacks. Supports reading PCM frames from the
 * M4A/ALAC container format.
 *
 * @param onRead                    Callback for reading data from the source.
 * @param onSeek                    Callback for seeking within the source.
 * @param onTell                    Callback for retrieving the current position (optional).
 * @param pReadSeekTellUserData     User data passed to I/O callbacks.
 * @param p_config                  Optional decoding backend configuration.
 * @param p_allocation_callbacks    Optional allocation callbacks (currently unused).
 * @param pM4a                      Pointer to the M4A decoder to initialize.
 *
 * @return MA_SUCCESS on success.
 * @return MA_INVALID_ARGS if required arguments are NULL.
 * @return MA_INVALID_FILE if the input data cannot be parsed as M4A.
 */
MA_API ma_result m4a_decoder_init(ma_read_proc onRead,
                                  ma_seek_proc onSeek,
                                  ma_tell_proc onTell,
                                  void *pReadSeekTellUserData,
                                  const ma_decoding_backend_config *p_config,
                                  const ma_allocation_callbacks *p_allocation_callbacks,
                                  m4a_decoder *pM4a);

/**
 * @brief Initializes an M4A decoder from a file on disk.
 *
 * Opens the specified file path and initializes decoding for the first
 * audio track (ALAC).
 *
 * @param pFilePath                 Path to the M4A file.
 * @param p_config                  Optional decoding backend configuration.
 * @param p_allocation_callbacks    Optional allocation callbacks (currently unused).
 * @param pM4a                      Pointer to the M4A decoder to initialize.
 *
 * @return MA_SUCCESS on success.
 * @return MA_INVALID_FILE if the file cannot be opened or parsed.
 */
MA_API ma_result m4a_decoder_init_file(const char *pFilePath,
                                       const ma_decoding_backend_config *p_config,
                                       const ma_allocation_callbacks *p_allocation_callbacks,
                                       m4a_decoder *pM4a);

/**
 * @brief Uninitializes an M4A decoder and releases associated resources.
 *
 * Frees decoder state and cleans up the underlying data source.
 *
 * @param pM4a                     Pointer to the M4A decoder.
 * @param p_allocation_callbacks    Optional allocation callbacks (currently unused).
 */
MA_API void m4a_decoder_uninit(m4a_decoder *pM4a,
                               const ma_allocation_callbacks *p_allocation_callbacks);

/**
 * @brief Reads decoded PCM frames from the M4A stream.
 *
 * Decodes audio packets and writes interleaved PCM frames to the
 * output buffer. Handles internal buffering and maintains a cursor
 * for subsequent reads.
 *
 * @param pM4a         Pointer to the M4A decoder.
 * @param p_frames_out Output buffer for decoded PCM frames.
 * @param frame_count  Number of PCM frames to read.
 * @param p_frames_read Optional pointer to receive the actual number of frames read.
 *
 * @return MA_SUCCESS on success.
 * @return MA_AT_END if the end of stream is reached.
 * @return MA_INVALID_ARGS if arguments are invalid.
 * @return MA_ERROR on decoding failure.
 */
MA_API ma_result m4a_decoder_read_pcm_frames(m4a_decoder *pM4a,
                                             void *p_frames_out,
                                             ma_uint64 frame_count,
                                             ma_uint64 *p_frames_read);

/**
 * @brief Seeks to a specific PCM frame in the M4A stream.
 *
 * Performs a container-level seek and resets internal decoder state.
 *
 * @param pM4a         Pointer to the M4A decoder.
 * @param frame_index  Target PCM frame index.
 *
 * @return MA_SUCCESS on success.
 * @return MA_INVALID_ARGS if arguments are invalid.
 * @return MA_INVALID_OPERATION if seeking fails.
 */
MA_API ma_result m4a_decoder_seek_to_pcm_frame(m4a_decoder *pM4a,
                                               ma_uint64 frame_index);

/**
 * @brief Retrieves the audio format of the decoded stream.
 *
 * Returns the sample format, channel count, sample rate, and optionally
 * fills a standard channel map.
 *
 * @param pM4a           Pointer to the M4A decoder.
 * @param p_format       Pointer to receive the sample format.
 * @param p_channels     Pointer to receive the number of channels.
 * @param p_sample_rate  Pointer to receive the sample rate.
 * @param p_channel_map  Optional buffer to receive the channel map.
 * @param channel_map_cap Capacity of the channel map buffer.
 *
 * @return MA_SUCCESS on success.
 * @return MA_INVALID_OPERATION if decoder is invalid.
 */
MA_API ma_result m4a_decoder_get_data_format(m4a_decoder *pM4a,
                                             ma_format *p_format,
                                             ma_uint32 *p_channels,
                                             ma_uint32 *p_sample_rate,
                                             ma_channel *p_channel_map,
                                             size_t channel_map_cap);

/**
 * @brief Retrieves the current playback cursor position in PCM frames.
 *
 * @param pM4a     Pointer to the M4A decoder.
 * @param p_cursor Pointer to receive the current PCM frame index.
 *
 * @return MA_SUCCESS on success.
 * @return MA_INVALID_ARGS if arguments are invalid.
 */
MA_API ma_result m4a_decoder_get_cursor_in_pcm_frames(m4a_decoder *pM4a,
                                                      ma_uint64 *p_cursor);

/**
 * @brief Retrieves the total length of the M4A stream in PCM frames.
 *
 * Computes and caches the total frame count based on container duration.
 *
 * @param pM4a    Pointer to the M4A decoder.
 * @param p_length Pointer to receive the total PCM frame length.
 *
 * @return MA_SUCCESS on success.
 * @return MA_INVALID_ARGS if arguments are invalid.
 */
MA_API ma_result m4a_decoder_get_length_in_pcm_frames(m4a_decoder *pM4a,
                                                      ma_uint64 *p_length);

/********************************************************************
 * Internal Data Source Adapter Functions
 * These wrap the M4A decoder API to implement the ma_data_source
 * interface.
 ********************************************************************/

/**
 * @brief Data source wrapper: retrieves the audio format from an M4A decoder.
 */
ma_result m4a_decoder_ds_get_data_format(ma_data_source *p_data_source,
                                         ma_format *p_format,
                                         ma_uint32 *p_channels,
                                         ma_uint32 *p_sample_rate,
                                         ma_channel *p_channel_map,
                                         size_t channel_map_cap);

/**
 * @brief Data source wrapper: reads PCM frames from an M4A decoder.
 */
ma_result m4a_decoder_ds_read(ma_data_source *p_data_source,
                              void *p_frames_out,
                              ma_uint64 frame_count,
                              ma_uint64 *p_frames_read);

/**
 * @brief Data source wrapper: seeks to a PCM frame in an M4A decoder.
 */
ma_result m4a_decoder_ds_seek(ma_data_source *p_data_source,
                              ma_uint64 frame_index);

/**
 * @brief Data source wrapper: retrieves the current cursor position in an M4A decoder.
 */
ma_result m4a_decoder_ds_get_cursor(ma_data_source *p_data_source,
                                    ma_uint64 *p_cursor);

/**
 * @brief Data source wrapper: retrieves the total length of the M4A decoder in PCM frames.
 */
ma_result m4a_decoder_ds_get_length(ma_data_source *p_data_source,
                                    ma_uint64 *p_length);

#if defined(MINIAUDIO_IMPLEMENTATION) || defined(MA_IMPLEMENTATION)

ma_result m4a_decoder_ds_read(ma_data_source *p_data_source, void *p_frames_out, ma_uint64 frame_count, ma_uint64 *p_frames_read)
{
        return m4a_decoder_read_pcm_frames((m4a_decoder *)p_data_source, p_frames_out, frame_count, p_frames_read);
}

ma_result m4a_decoder_ds_seek(ma_data_source *p_data_source, ma_uint64 frame_index)
{
        return m4a_decoder_seek_to_pcm_frame((m4a_decoder *)p_data_source, frame_index);
}

ma_result m4a_decoder_ds_get_data_format(ma_data_source *p_data_source, ma_format *p_format, ma_uint32 *p_channels, ma_uint32 *p_sample_rate, ma_channel *p_channel_map, size_t channel_map_cap)
{
        return m4a_decoder_get_data_format((m4a_decoder *)p_data_source, p_format, p_channels, p_sample_rate, p_channel_map, channel_map_cap);
}

ma_result m4a_decoder_ds_get_cursor(ma_data_source *p_data_source, ma_uint64 *p_cursor)
{
        return m4a_decoder_get_cursor_in_pcm_frames((m4a_decoder *)p_data_source, p_cursor);
}

ma_result m4a_decoder_ds_get_length(ma_data_source *p_data_source, ma_uint64 *p_length)
{
        return m4a_decoder_get_length_in_pcm_frames((m4a_decoder *)p_data_source, p_length);
}

ma_data_source_vtable g_m4a_decoder_ds_vtable =
    {
        m4a_decoder_ds_read,
        m4a_decoder_ds_seek,
        m4a_decoder_ds_get_data_format,
        m4a_decoder_ds_get_cursor,
        m4a_decoder_ds_get_length,
        NULL,
        (ma_uint64)0};

static ma_result m4a_decoder_init_internal(const ma_decoding_backend_config *p_config, m4a_decoder *pM4a)
{
        if (pM4a == NULL) {
                return MA_INVALID_ARGS;
        }

        MA_ZERO_OBJECT(pM4a);
        pM4a->format = ma_format_f32;

        if (p_config != NULL && (p_config->preferredFormat == ma_format_f32 || p_config->preferredFormat == ma_format_s16)) {
                pM4a->format = p_config->preferredFormat;
        }

        ma_data_source_config dataSourceConfig = ma_data_source_config_init();

        dataSourceConfig.vtable = &g_m4a_decoder_ds_vtable;

        ma_result result = ma_data_source_init(&dataSourceConfig, &pM4a->ds);
        if (result != MA_SUCCESS) {
                return result;
        }

        return MA_SUCCESS;
}

// Note: This isn't used by kew and is untested
MA_API ma_result m4a_decoder_init(
    ma_read_proc onRead,
    ma_seek_proc onSeek,
    ma_tell_proc onTell,
    void *pReadSeekTellUserData,
    const ma_decoding_backend_config *p_config,
    const ma_allocation_callbacks *p_allocation_callbacks,
    m4a_decoder *pM4a)
{
        (void)onRead;
        (void)onSeek;
        (void)onTell;
        (void)pReadSeekTellUserData;
        (void)p_config;
        (void)p_allocation_callbacks;
        (void)pM4a;

        /*
        if (pM4a == NULL || onRead == NULL || onSeek == NULL || onTell == NULL) {
                return MA_INVALID_ARGS;
        }

        ma_result result = m4a_decoder_init_internal(p_config, pM4a);
        if (result != MA_SUCCESS) {
                return result;
        }

        // Store the custom read, seek, and tell functions
        pM4a->pReadSeekTellUserData = pReadSeekTellUserData;

        // Get the size of the data source
        ma_int64 currentPos = 0;
        if (pM4a->onTell(pM4a->pReadSeekTellUserData, &currentPos) != MA_SUCCESS) {
                return MA_ERROR;
        }

        if (pM4a->onSeek(pM4a->pReadSeekTellUserData, 0, ma_seek_origin_end) != MA_SUCCESS) {
                return MA_ERROR;
        }

        ma_int64 file_size = 0;
        if (pM4a->onTell(pM4a->pReadSeekTellUserData, &file_size) != MA_SUCCESS) {
                return MA_ERROR;
        }

        // Seek back to original position
        if (pM4a->onSeek(pM4a->pReadSeekTellUserData, currentPos, ma_seek_origin_start) != MA_SUCCESS) {
                return MA_ERROR;
        }

        // Initialize mp4lib
        if (mp4_demux_open(filename, &pM4a->mp4) < 0)
                return MA_ERROR;

        pM4a->buffer = NULL;

        // Find the audio track
        pM4a->audio_track_index = -1;
        for (unsigned int i = 0; i < pM4a->mp4->track_count; i++) {
                MP4D_track_t *track = &pM4a->mp4.track[i];
                if (track->handler_type == MP4D_HANDLER_TYPE_SOUN) {
                        pM4a->audio_track_index = i;
                        pM4a->track = track;
                        break;
                }
        }

        if (pM4a->audio_track_index == -1) {
                // No audio track found
                mp4_demux_close(pM4a->mp4);
                return MA_ERROR;
        }

        pM4a->current_sample = 0;
        pM4a->total_samples = pM4a->track->sample_count;

        // Initialize faad2 decoder
        pM4a->hDecoder = NeAACDecOpen();

        // Extract the decoder configuration
        const uint8_t *decoder_config = pM4a->track->dsi;
        uint32_t decoder_config_len = pM4a->track->dsi_bytes;

        unsigned long sample_rate;
        unsigned char channels;

        if (NeAACDecInit2(pM4a->hDecoder, (unsigned char *)decoder_config, decoder_config_len, &sample_rate, &channels) < 0) {
                // Error initializing decoder
                NeAACDecClose(pM4a->hDecoder);
                mp4_demux_close(pM4a->mp4);
                return MA_ERROR;
        }

        // Configure output format
        NeAACDecConfigurationPtr config = NeAACDecGetCurrentConfiguration(pM4a->hDecoder);
        if (pM4a->format == ma_format_s16) {
                config->outputFormat = FAAD_FMT_16BIT;
                pM4a->sampleSize = sizeof(int16_t);
                pM4a->bit_depth = 16;
        } else if (pM4a->format == ma_format_f32) {
                config->outputFormat = FAAD_FMT_FLOAT;
                pM4a->sampleSize = sizeof(float);
                pM4a->bit_depth = 32;
        } else {
                // Unsupported format
                NeAACDecClose(pM4a->hDecoder);
                mp4_demux_close(pM4a->mp4);
                return MA_ERROR;
        }
        NeAACDecSetConfiguration(pM4a->hDecoder, config);

        // Initialize other fields
        pM4a->leftoverSampleCount = 0;
        pM4a->cursor = 0;
        */

        k_log(" m4a_decoder_init: Not Implemented");

        exit(0);

        return MA_SUCCESS;
}

static ma_result build_adts_index(m4a_decoder *decoder)
{
        if (decoder == NULL || decoder->file == NULL || decoder->file_size <= 0)
                return MA_INVALID_ARGS;

        uint32_t capacity = 0;
        uint64_t offset = 0;
        uint64_t pcm_offset = 0;
        while (offset < (uint64_t)decoder->file_size) {
                uint8_t header[7];

                if ((uint64_t)decoder->file_size - offset < sizeof(header) ||
                    fseeko(decoder->file, (off_t)offset, SEEK_SET) != 0 ||
                    fread(header, 1, sizeof(header), decoder->file) !=
                        sizeof(header))
                        return MA_INVALID_FILE;
                if (header[0] != 0xff || (header[1] & 0xf6) != 0xf0)
                        return MA_INVALID_FILE;

                // lowest bit of header[1] is the protection_absent flag
                // that shows if CRC protection is present or not
                // if its protected then header is 9 bytes, otherwise 7 bytes
                uint32_t header_size = (header[1] & 1) ? 7u : 9u;

                uint32_t frame_size =
                    ((uint32_t)(header[3] & 3) << 11) |
                    ((uint32_t)header[4] << 3) |
                    ((uint32_t)(header[5] & 0xe0) >> 5);

                if (frame_size < header_size ||
                    frame_size > 8191 ||            // frame length = 13 bits & largest 13 bit number = 8191
                    frame_size > (uint64_t)decoder->file_size - offset)
                        return MA_INVALID_FILE;

                // filled all currently allocated index slots - allocate more slots
                if (decoder->adts_frame_count == capacity) {
                        uint32_t new_capacity = capacity ? capacity * 2 : 1024;
                        size_t allocation_size =
                            (size_t)new_capacity * sizeof(uint64_t);
                        if (new_capacity < capacity ||
                            allocation_size / sizeof(uint64_t) != new_capacity)
                                return MA_OUT_OF_MEMORY;
                        void *new_offsets = realloc(
                            decoder->adts_frame_offsets,
                            allocation_size);
                        if (new_offsets == NULL)
                                return MA_OUT_OF_MEMORY;
                        decoder->adts_frame_offsets = new_offsets;
                        void *new_pcm_offsets = realloc(
                            decoder->adts_pcm_offsets,
                            allocation_size);
                        if (new_pcm_offsets == NULL)
                                return MA_OUT_OF_MEMORY;
                        decoder->adts_pcm_offsets = new_pcm_offsets;
                        capacity = new_capacity;
                }

                decoder->adts_frame_offsets[decoder->adts_frame_count] = offset;
                decoder->adts_pcm_offsets[decoder->adts_frame_count] = pcm_offset;
                decoder->adts_frame_count++;
                // lowest 2 bytes of header[6] hold number_of_raw_data_blocks_in_frame - 1
                uint64_t frame_pcm_count = ((uint64_t)(header[6] & 3) + 1) * 1024;
                if (pcm_offset > UINT64_MAX - frame_pcm_count)
                        return MA_OUT_OF_MEMORY;
                pcm_offset += frame_pcm_count;
                offset += frame_size;
        }

        if (decoder->adts_frame_count == 0)
                return MA_INVALID_FILE;
        decoder->adts_total_pcm_frames = pcm_offset;
        decoder->totalFrames = decoder->adts_frame_count;
        decoder->duration = (double)pcm_offset / decoder->sample_rate;
        if (decoder->duration > 0.0)
                decoder->avg_bit_rate = (ma_uint32)
                    (((double)decoder->file_size * 8.0) / decoder->duration);
        if (fseeko(decoder->file, 0, SEEK_SET) != 0)
                return MA_ERROR;
        return MA_SUCCESS;
}

uint32_t read_u32be(FILE *fp)
{
        unsigned char b[4];
        if (fread(b, 1, 4, fp) != 4)
                return 0;
        return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | ((uint32_t)b[3]);
}

int find_atom(FILE *fp, uint32_t atom_name, long max_search_length, uint32_t *atom_size_out)
{
        long start_pos = ftell(fp);
        while ((ftell(fp) - start_pos) < max_search_length) {
                unsigned char header[8];
                if (fread(header, 1, 8, fp) != 8)
                        return 0;

                uint32_t atom_size = (header[0] << 24) | (header[1] << 16) | (header[2] << 8) | header[3];
                uint32_t atom_type = (header[4] << 24) | (header[5] << 16) | (header[6] << 8) | header[7];

                if (atom_size < 8)
                        return 0; // Invalid atom size

                if (atom_type == atom_name) {
                        if (atom_size_out)
                                *atom_size_out = atom_size - 8;

                        return 1; // Found
                }

                if (fseek(fp, atom_size - 8, SEEK_CUR) != 0)
                        return 0;
        }
        return 0; // Not found
}

int is_alac(FILE *fp, uint8_t *dsi_out, size_t *dsi_size_out)
{
        fseek(fp, 0, SEEK_SET);
        uint32_t atom_size;

        if (!find_atom(fp, FOUR_CHAR_INT('m', 'o', 'o', 'v'), 0x7FFFFFFF, &atom_size))
                return 0;
        if (!find_atom(fp, FOUR_CHAR_INT('t', 'r', 'a', 'k'), atom_size, &atom_size))
                return 0;
        if (!find_atom(fp, FOUR_CHAR_INT('m', 'd', 'i', 'a'), atom_size, &atom_size))
                return 0;
        if (!find_atom(fp, FOUR_CHAR_INT('m', 'i', 'n', 'f'), atom_size, &atom_size))
                return 0;
        if (!find_atom(fp, FOUR_CHAR_INT('s', 't', 'b', 'l'), atom_size, &atom_size))
                return 0;
        if (!find_atom(fp, FOUR_CHAR_INT('s', 't', 's', 'd'), atom_size, &atom_size))
                return 0;

        fseek(fp, 8, SEEK_CUR); // Skip stsd header (version+entry)

        read_u32be(fp); // uint32_t sample_entry_size
        uint32_t sample_entry_fourcc = read_u32be(fp);
        if (sample_entry_fourcc != FOUR_CHAR_INT('a', 'l', 'a', 'c'))
                return 0;

        fseek(fp, 28, SEEK_CUR); // Skip audio sample entry fields

        uint32_t config_atom_size = read_u32be(fp);
        uint32_t config_atom_fourcc = read_u32be(fp);
        if (config_atom_fourcc != FOUR_CHAR_INT('a', 'l', 'a', 'c'))
                return 0;

        fseek(fp, 4, SEEK_CUR); // Skip 1-byte version and 3-byte flags (4 bytes total)!

        uint32_t alac_dsi_size = config_atom_size - 12; // size(4)+fourcc(4)+version/flags(4) total=12 bytes overhead
        if (alac_dsi_size < 24 || alac_dsi_size > 64)
                return 0; // Sanity check

        if (fread(dsi_out, 1, alac_dsi_size, fp) != alac_dsi_size)
                return 0;
        *dsi_size_out = alac_dsi_size;

        return 1;
}

MA_API ma_result m4a_decoder_init_file(
    const char *pFilePath,
    const ma_decoding_backend_config *p_config,
    const ma_allocation_callbacks *p_allocation_callbacks,
    m4a_decoder *pM4a)
{
        FILE *fp = NULL;

        (void)p_allocation_callbacks;

        if (pFilePath == NULL || pM4a == NULL) {
                return MA_INVALID_ARGS;
        }

        ma_result result = m4a_decoder_init_internal(p_config, pM4a);
        if (result != MA_SUCCESS) {
                return result;
        }

        pM4a->file = NULL;
        pM4a->mp4 = NULL;
        pM4a->hDecoder = NULL;
        pM4a->alac = NULL;
        pM4a->buffer = NULL;
        pM4a->buffer_size = 0;
        pM4a->alacScratch = NULL;
        pM4a->alacScratchSize = 0;

        pM4a->file_size = 0;
        pM4a->file_type = k_unknown;

        pM4a->audio_track_index = -1;
        pM4a->audio_track_id = 0;

        pM4a->current_sample = 0;
        pM4a->total_samples = 0;
        pM4a->adts_frame_offsets = NULL;
        pM4a->adts_pcm_offsets = NULL;
        pM4a->adts_frame_count = 0;
        pM4a->adts_total_pcm_frames = 0;

        pM4a->leftoverSampleCount = 0;
        pM4a->cursor = 0;

        pM4a->sample_rate = 0;
        pM4a->channels = 0;
        pM4a->sampleSize = 0;
        pM4a->bit_depth = 0;
        pM4a->avg_bit_rate = 0;

        fp = fopen(pFilePath, "rb");
        if (fp == NULL) {
                return MA_INVALID_FILE;
        }

        // Get the file size
        if (fseeko(fp, 0, SEEK_END) != 0) {
                fclose(fp);
                return MA_ERROR;
        }

        pM4a->file_size = ftello(fp);
        if (pM4a->file_size < 0) {
                fclose(fp);
                return MA_ERROR;
        }

        if (fseeko(fp, 0, SEEK_SET) != 0) {
                fclose(fp);
                return MA_ERROR;
        }

        // Store the FILE pointer in the decoder struct
        pM4a->file = fp;

        // Try to detect the file format (ADTS, MP4, LATM, etc.)
        {
                unsigned char header[7];
                size_t bytes_read = fread(header, 1, sizeof(header), fp);

                /*
                * ADTS syncword:
                *
                *   12 bits: 1111 1111 1111
                *
                * The second byte's top four bits must therefore be 0xF.
                */
                if (bytes_read == sizeof(header) &&
                    header[0] == 0xFF &&
                    (header[1] & 0xF0) == 0xF0 &&
                    (header[1] & 0x06) == 0x00) {

                        // Raw ADTS AAC.
                        pM4a->file_type = k_rawAAC;

                        // Validate the ADTS header before using fields from it.
                        unsigned int frameSize =
                            ((unsigned int)(header[3] & 0x03) << 11) |
                            ((unsigned int)header[4] << 3) |
                            ((unsigned int)(header[5] & 0xE0) >> 5);

                        if (frameSize < 7 || frameSize > 8192) {
                                fclose(fp);
                                pM4a->file = NULL;
                                return MA_ERROR;
                        }

                        /*
                        * Build the AudioSpecificConfig from the ADTS header.
                        *
                        * profile:
                        *   ADTS profile is object_type - 1.
                        *
                        * sampling_frequency_index:
                        *   header[2] bits 5..2.
                        *
                        * channel_configuration:
                        *   header[2] bits 1..0 + header[3] bit 7.
                        */
                        unsigned int profile =
                            ((unsigned int)(header[2] & 0xC0) >> 6) + 1;

                        unsigned int sampleRateIndex =
                            (unsigned int)((header[2] & 0x3C) >> 2);

                        unsigned int channelConfiguration =
                            ((unsigned int)(header[2] & 0x01) << 2) |
                            ((unsigned int)(header[3] & 0xC0) >> 6);

                        /*
                        * The above expression deserves special attention:
                        *
                        * ADTS channel_configuration is:
                        *
                        *   header[2] bit 0
                        *   header[3] bits 7..6
                        *
                        * Therefore:
                        */
                        channelConfiguration =
                            ((unsigned int)(header[2] & 0x01) << 2) |
                            ((unsigned int)(header[3] & 0xC0) >> 6);

                        /*
                        * FAAD2 AudioSpecificConfig is two bytes for the normal AAC-LC
                        * configurations handled here.
                        */
                        unsigned char decoder_config[2];

                        decoder_config[0] =
                            (unsigned char)((profile << 3) |
                                            ((sampleRateIndex & 0x0E) >> 1));

                        decoder_config[1] =
                            (unsigned char)(((sampleRateIndex & 0x01) << 7) |
                                            (channelConfiguration << 3));

                        /*
                        * Only AAC-LC is supported by the current implementation.
                        *
                        * In ADTS the profile field is object_type - 1.
                        * AAC LC therefore has profile == 2.
                        */
                        if (profile != 2) {
                                k_log("Unsupported ADTS AAC profile: %u\n", profile);
                                set_error_message(
                                    "The current or next file uses an unsupported AAC profile.");
                                fclose(fp);
                                pM4a->file = NULL;
                                return MA_ERROR;
                        }

                        /*
                        * The channel configuration must be non-zero.
                        *
                        * Configuration 0 means that a Program Config Element supplies
                        * the channel configuration, which this initialization path does
                        * not currently parse.
                        */
                        if (channelConfiguration == 0) {
                                k_log("Unsupported ADTS channel configuration 0\n");
                                set_error_message(
                                    "The current or next file uses an unsupported AAC channel configuration.");
                                fclose(fp);
                                pM4a->file = NULL;
                                return MA_ERROR;
                        }

                        /*
                        * Initialize FAAD2.
                        */
                        pM4a->hDecoder = NeAACDecOpen();
                        if (pM4a->hDecoder == NULL) {
                                fclose(fp);
                                pM4a->file = NULL;
                                return MA_OUT_OF_MEMORY;
                        }

                        unsigned long sample_rate = 0;
                        unsigned char channels = 0;

                        int initResult =
                            NeAACDecInit2(
                                pM4a->hDecoder,
                                decoder_config,
                                sizeof(decoder_config),
                                &sample_rate,
                                &channels);

                        if (initResult < 0) {
                                k_log("Error initializing decoder. Code: %d\n", initResult);
                                set_error_message(
                                    "Error initializing decoder in the current or next file.");

                                NeAACDecClose(pM4a->hDecoder);
                                pM4a->hDecoder = NULL;

                                fclose(fp);
                                pM4a->file = NULL;

                                return MA_ERROR;
                        }

                        if (sample_rate == 0 || channels == 0) {
                                k_log("Invalid sample rate or channel count.\n");
                                set_error_message(
                                    "The current or next file contains an invalid sample rate or channel count.");

                                NeAACDecClose(pM4a->hDecoder);
                                pM4a->hDecoder = NULL;

                                fclose(fp);
                                pM4a->file = NULL;

                                return MA_ERROR;
                        }

                        pM4a->sample_rate = (ma_uint32)sample_rate;
                        pM4a->channels = (ma_uint32)channels;

                        /*
                        * Configure FAAD2 output.
                        */
                        NeAACDecConfigurationPtr config_ptr =
                            NeAACDecGetCurrentConfiguration(pM4a->hDecoder);

                        if (config_ptr == NULL) {
                                NeAACDecClose(pM4a->hDecoder);
                                pM4a->hDecoder = NULL;

                                fclose(fp);
                                pM4a->file = NULL;

                                return MA_ERROR;
                        }

                        if (pM4a->format == ma_format_s16) {
                                config_ptr->outputFormat = FAAD_FMT_16BIT;
                                pM4a->sampleSize = sizeof(int16_t);
                                pM4a->bit_depth = 16;
                        } else if (pM4a->format == ma_format_f32) {
                                config_ptr->outputFormat = FAAD_FMT_FLOAT;
                                pM4a->sampleSize = sizeof(float);
                                pM4a->bit_depth = 32;
                        } else {
                                NeAACDecClose(pM4a->hDecoder);
                                pM4a->hDecoder = NULL;

                                fclose(fp);
                                pM4a->file = NULL;

                                return MA_FORMAT_NOT_SUPPORTED;
                        }

                        if (!NeAACDecSetConfiguration(pM4a->hDecoder, config_ptr)) {
                                NeAACDecClose(pM4a->hDecoder);
                                pM4a->hDecoder = NULL;

                                fclose(fp);
                                pM4a->file = NULL;

                                return MA_ERROR;
                        }

                        /*
                        * We only consumed the 7-byte detection header. Return to the
                        * beginning so read_pcm_frames() sees the first ADTS frame.
                        */
                        if (fseeko(fp, 0, SEEK_SET) != 0) {
                                NeAACDecClose(pM4a->hDecoder);
                                pM4a->hDecoder = NULL;

                                fclose(fp);
                                pM4a->file = NULL;

                                return MA_ERROR;
                        }

                        pM4a->leftoverSampleCount = 0;
                        pM4a->cursor = 0;

                        ma_result index_result = build_adts_index(pM4a);
                        if (index_result != MA_SUCCESS) {
                                m4a_decoder_uninit(pM4a, NULL);
                                return index_result;
                        }

                        return MA_SUCCESS;
                }
        }
        /*
        * Not ADTS. Try opening it as an MP4/M4A container.
        */
        if (mp4_demux_open(pFilePath, &pM4a->mp4) < 0) {
                fclose(fp);
                pM4a->file = NULL;

                k_log("Error initializing decoder.\n");
                set_error_message(
                    "Error initializing decoder (possibly a fragmented mp4 file) "
                    "in the current or next file.");

                return MA_ERROR;
        }

        /*
     * Find the first audio track.
     */
        int trackCount = mp4_demux_get_track_count(pM4a->mp4);
        if (trackCount <= 0) {
                mp4_demux_close(pM4a->mp4);
                pM4a->mp4 = NULL;

                fclose(fp);
                pM4a->file = NULL;

                return MA_ERROR;
        }

        pM4a->audio_track_index = -1;

        for (int i = 0; i < trackCount; ++i) {
                struct mp4_track_info info;

                if (mp4_demux_get_track_info(pM4a->mp4, i, &info) < 0) {
                        continue;
                }

                if (info.type != MP4_TRACK_TYPE_AUDIO) {
                        continue;
                }

                if (info.sample_max_size == 0 || info.sample_count == 0) {
                        continue;
                }

                pM4a->audio_track_index = i;
                pM4a->audio_track_id = info.id;
                pM4a->track = info;

                pM4a->buffer = malloc(info.sample_max_size);
                if (pM4a->buffer == NULL) {
                        mp4_demux_close(pM4a->mp4);
                        pM4a->mp4 = NULL;

                        fclose(fp);
                        pM4a->file = NULL;

                        return MA_OUT_OF_MEMORY;
                }

                pM4a->buffer_size = info.sample_max_size;

                break;
        }

        if (pM4a->audio_track_index == -1) {
                mp4_demux_close(pM4a->mp4);
                pM4a->mp4 = NULL;

                fclose(fp);
                pM4a->file = NULL;

                return MA_ERROR;
        }

        pM4a->current_sample = 0;
        pM4a->total_samples = pM4a->track.sample_count;

        /*
     * Calculate average bitrate from container duration.
     */
        uint64_t duration_us =
            mp4_sample_time_to_usec(
                pM4a->track.duration,
                pM4a->track.timescale);

        if (duration_us > 0 && pM4a->file_size > 0) {
                uint64_t bps =
                    ((uint64_t)pM4a->file_size * 8ULL * 1000000ULL) /
                    duration_us;

                pM4a->avg_bit_rate = (ma_uint32)((bps + 500ULL) / 1000ULL);
        }

        /*
     * Determine whether the audio track is ALAC.
     */
        {
                uint8_t alac_dsi[32];
                size_t alac_dsi_size = 0;

                if (fseeko(fp, 0, SEEK_SET) != 0) {
                        free(pM4a->buffer);
                        pM4a->buffer = NULL;
                        pM4a->buffer_size = 0;

                        mp4_demux_close(pM4a->mp4);
                        pM4a->mp4 = NULL;

                        fclose(fp);
                        pM4a->file = NULL;

                        return MA_ERROR;
                }

                if (is_alac(fp, alac_dsi, &alac_dsi_size)) {
                        pM4a->file_type = k_ALAC;

                        if (alac_dsi_size < 24) {
                                k_log("ALAC config too short.");

                                free(pM4a->buffer);
                                pM4a->buffer = NULL;
                                pM4a->buffer_size = 0;

                                mp4_demux_close(pM4a->mp4);
                                pM4a->mp4 = NULL;

                                fclose(fp);
                                pM4a->file = NULL;

                                return MA_ERROR;
                        }

                        uint8_t bitDepth = alac_dsi[5];
                        uint8_t numCh = alac_dsi[9];

                        if ((bitDepth != 16 && bitDepth != 24) ||
                            numCh == 0 ||
                            numCh > 8) {

                                k_log("Unsupported ALAC bit depth or channel count.");
                                set_error_message(
                                    "The current or next file uses an unsupported ALAC format.");

                                free(pM4a->buffer);
                                pM4a->buffer = NULL;
                                pM4a->buffer_size = 0;

                                mp4_demux_close(pM4a->mp4);
                                pM4a->mp4 = NULL;

                                fclose(fp);
                                pM4a->file = NULL;

                                return MA_ERROR;
                        }

                        pM4a->alac = alac_create(bitDepth, numCh);
                        if (pM4a->alac == NULL) {
                                k_log("Failed to create ALAC decoder.");

                                free(pM4a->buffer);
                                pM4a->buffer = NULL;
                                pM4a->buffer_size = 0;

                                mp4_demux_close(pM4a->mp4);
                                pM4a->mp4 = NULL;

                                fclose(fp);
                                pM4a->file = NULL;

                                return MA_OUT_OF_MEMORY;
                        }

                        /*
             * alac_set_info() expects 24 bytes of leading data before the
             * actual ALAC configuration.
             */
                        unsigned char cookie[24 + 64];
                        memset(cookie, 0, sizeof(cookie));

                        size_t copySize =
                            alac_dsi_size < 64 ? alac_dsi_size : 64;

                        memcpy(cookie + 24, alac_dsi, copySize);

                        alac_set_info(pM4a->alac, (char *)cookie);

                        pM4a->sample_rate =
                            pM4a->alac->setinfo_8a_rate;

                        pM4a->channels =
                            pM4a->alac->setinfo_7f;

                        if (pM4a->sample_rate == 0 ||
                            pM4a->channels == 0 ||
                            pM4a->channels > 8) {

                                k_log("Invalid ALAC sample rate or channel count.");

                                alac_free(pM4a->alac);
                                pM4a->alac = NULL;

                                free(pM4a->buffer);
                                pM4a->buffer = NULL;
                                pM4a->buffer_size = 0;

                                mp4_demux_close(pM4a->mp4);
                                pM4a->mp4 = NULL;

                                fclose(fp);
                                pM4a->file = NULL;

                                return MA_ERROR;
                        }

                        if (pM4a->format == ma_format_s16) {
                                pM4a->sampleSize = sizeof(int16_t);
                                pM4a->bit_depth = 16;
                        } else if (pM4a->format == ma_format_f32) {
                                pM4a->sampleSize = sizeof(float);
                                pM4a->bit_depth = 32;
                        } else {
                                alac_free(pM4a->alac);
                                pM4a->alac = NULL;

                                free(pM4a->buffer);
                                pM4a->buffer = NULL;
                                pM4a->buffer_size = 0;

                                mp4_demux_close(pM4a->mp4);
                                pM4a->mp4 = NULL;

                                fclose(fp);
                                pM4a->file = NULL;

                                return MA_FORMAT_NOT_SUPPORTED;
                        }

                        /*
             * The ALAC decoder produces its native PCM representation into
             * this scratch buffer. bytespersample is the number of bytes
             * per interleaved sample.
             */
                        pM4a->alacScratchSize =
                            (size_t)pM4a->alac->setinfo_max_samples_per_frame *
                            (size_t)pM4a->alac->bytespersample;

                        if (pM4a->alacScratchSize == 0) {
                                alac_free(pM4a->alac);
                                pM4a->alac = NULL;

                                free(pM4a->buffer);
                                pM4a->buffer = NULL;
                                pM4a->buffer_size = 0;

                                mp4_demux_close(pM4a->mp4);
                                pM4a->mp4 = NULL;

                                fclose(fp);
                                pM4a->file = NULL;

                                return MA_ERROR;
                        }

                        pM4a->alacScratch = malloc(pM4a->alacScratchSize);
                        if (pM4a->alacScratch == NULL) {
                                alac_free(pM4a->alac);
                                pM4a->alac = NULL;

                                free(pM4a->buffer);
                                pM4a->buffer = NULL;
                                pM4a->buffer_size = 0;

                                mp4_demux_close(pM4a->mp4);
                                pM4a->mp4 = NULL;

                                fclose(fp);
                                pM4a->file = NULL;

                                return MA_OUT_OF_MEMORY;
                        }

                        pM4a->leftoverSampleCount = 0;
                        pM4a->cursor = 0;

                        return MA_SUCCESS;
                }
        }

        /*
     * Otherwise the track is AAC.
     */
        pM4a->file_type = k_aac;

        pM4a->hDecoder = NeAACDecOpen();
        if (pM4a->hDecoder == NULL) {
                free(pM4a->buffer);
                pM4a->buffer = NULL;
                pM4a->buffer_size = 0;

                mp4_demux_close(pM4a->mp4);
                pM4a->mp4 = NULL;

                fclose(fp);
                pM4a->file = NULL;

                return MA_OUT_OF_MEMORY;
        }

        uint8_t *decoder_config = NULL;
        unsigned int decoder_config_len = 0;

        if (mp4_demux_get_track_audio_specific_config(
                pM4a->mp4,
                pM4a->track.id,
                &decoder_config,
                &decoder_config_len) != 0 ||
            decoder_config == NULL ||
            decoder_config_len < 2) {

                k_log("Unsupported codec.");
                set_error_message(
                    "The current or next file uses an unsupported codec.");

                NeAACDecClose(pM4a->hDecoder);
                pM4a->hDecoder = NULL;

                free(pM4a->buffer);
                pM4a->buffer = NULL;
                pM4a->buffer_size = 0;

                mp4_demux_close(pM4a->mp4);
                pM4a->mp4 = NULL;

                fclose(fp);
                pM4a->file = NULL;

                return MA_ERROR;
        }

        /*
     * AudioSpecificConfig object type.
     *
     * This handles the normal two-byte AAC configuration. Extended object
     * types require additional parsing and are deliberately rejected.
     */
        uint8_t object_type =
            (uint8_t)((decoder_config[0] >> 3) & 0x1F);

        if (object_type == 5 || object_type == 29) {
                k_log("Unsupported AAC object type: %u\n", object_type);
                set_error_message(
                    "The current or next file uses an unsupported AAC object type "
                    "(HE-AAC or PS).");

                NeAACDecClose(pM4a->hDecoder);
                pM4a->hDecoder = NULL;

                free(pM4a->buffer);
                pM4a->buffer = NULL;
                pM4a->buffer_size = 0;

                mp4_demux_close(pM4a->mp4);
                pM4a->mp4 = NULL;

                fclose(fp);
                pM4a->file = NULL;

                return MA_ERROR;
        }

        unsigned long sample_rate = 0;
        unsigned char channels = 0;

        if (NeAACDecInit2(
                pM4a->hDecoder,
                decoder_config,
                decoder_config_len,
                &sample_rate,
                &channels) < 0) {

                k_log("Error initializing AAC decoder.");
                set_error_message(
                    "Error initializing decoder in the current or next file.");

                NeAACDecClose(pM4a->hDecoder);
                pM4a->hDecoder = NULL;

                free(pM4a->buffer);
                pM4a->buffer = NULL;
                pM4a->buffer_size = 0;

                mp4_demux_close(pM4a->mp4);
                pM4a->mp4 = NULL;

                fclose(fp);
                pM4a->file = NULL;

                return MA_ERROR;
        }

        if (sample_rate == 0 || channels == 0) {
                NeAACDecClose(pM4a->hDecoder);
                pM4a->hDecoder = NULL;

                free(pM4a->buffer);
                pM4a->buffer = NULL;
                pM4a->buffer_size = 0;

                mp4_demux_close(pM4a->mp4);
                pM4a->mp4 = NULL;

                fclose(fp);
                pM4a->file = NULL;

                return MA_ERROR;
        }

        pM4a->sample_rate = (ma_uint32)sample_rate;
        pM4a->channels = (ma_uint32)channels;

        NeAACDecConfigurationPtr config_ptr =
            NeAACDecGetCurrentConfiguration(pM4a->hDecoder);

        if (config_ptr == NULL) {
                NeAACDecClose(pM4a->hDecoder);
                pM4a->hDecoder = NULL;

                free(pM4a->buffer);
                pM4a->buffer = NULL;
                pM4a->buffer_size = 0;

                mp4_demux_close(pM4a->mp4);
                pM4a->mp4 = NULL;

                fclose(fp);
                pM4a->file = NULL;

                return MA_ERROR;
        }

        if (pM4a->format == ma_format_s16) {
                config_ptr->outputFormat = FAAD_FMT_16BIT;
                pM4a->sampleSize = sizeof(int16_t);
                pM4a->bit_depth = 16;
        } else if (pM4a->format == ma_format_f32) {
                config_ptr->outputFormat = FAAD_FMT_FLOAT;
                pM4a->sampleSize = sizeof(float);
                pM4a->bit_depth = 32;
        } else {
                NeAACDecClose(pM4a->hDecoder);
                pM4a->hDecoder = NULL;

                free(pM4a->buffer);
                pM4a->buffer = NULL;
                pM4a->buffer_size = 0;

                mp4_demux_close(pM4a->mp4);
                pM4a->mp4 = NULL;

                fclose(fp);
                pM4a->file = NULL;

                return MA_FORMAT_NOT_SUPPORTED;
        }

        if (!NeAACDecSetConfiguration(pM4a->hDecoder, config_ptr)) {
                NeAACDecClose(pM4a->hDecoder);
                pM4a->hDecoder = NULL;

                free(pM4a->buffer);
                pM4a->buffer = NULL;
                pM4a->buffer_size = 0;

                mp4_demux_close(pM4a->mp4);
                pM4a->mp4 = NULL;

                fclose(fp);
                pM4a->file = NULL;

                return MA_ERROR;
        }

        pM4a->leftoverSampleCount = 0;
        pM4a->cursor = 0;

        if (fseeko(fp, 0, SEEK_SET) != 0) {
                NeAACDecClose(pM4a->hDecoder);
                pM4a->hDecoder = NULL;

                free(pM4a->buffer);
                pM4a->buffer = NULL;
                pM4a->buffer_size = 0;

                mp4_demux_close(pM4a->mp4);
                pM4a->mp4 = NULL;

                fclose(fp);
                pM4a->file = NULL;

                return MA_ERROR;
        }

        return MA_SUCCESS;
}

MA_API void m4a_decoder_uninit(m4a_decoder *pM4a, const ma_allocation_callbacks *p_allocation_callbacks)
{
        (void)p_allocation_callbacks;

        if (pM4a == NULL) {
                return;
        }

        if (pM4a->hDecoder) {
                NeAACDecClose(pM4a->hDecoder);
                pM4a->hDecoder = NULL;
        }

        if (pM4a->file_type != k_rawAAC) {
                mp4_demux_close(pM4a->mp4);
        }

        if (pM4a->file_type == k_ALAC) {
                if (pM4a->alac) {
                        alac_free(pM4a->alac);
                        pM4a->alac = NULL;
                }
                if (pM4a->alacScratch) {
                        free(pM4a->alacScratch);
                        pM4a->alacScratch = NULL;
                }
        }

        if (pM4a->buffer) {
                free(pM4a->buffer);
                pM4a->buffer = NULL;
                pM4a->buffer_size = 0;
        }

        free(pM4a->adts_frame_offsets);
        pM4a->adts_frame_offsets = NULL;
        free(pM4a->adts_pcm_offsets);
        pM4a->adts_pcm_offsets = NULL;
        pM4a->adts_frame_count = 0;
        pM4a->adts_total_pcm_frames = 0;

        if (pM4a->file) {
                fclose(pM4a->file);
                pM4a->file = NULL;
        }
}

static void alac_pcm16_to_output(const int16_t *src, void *dst, ma_uint32 frames,
                                 ma_uint32 channels, ma_format outFmt)
{
        ma_uint32 n = frames * channels;
        if (outFmt == ma_format_s16) {
                memcpy(dst, src, (size_t)n * sizeof(int16_t));
        } else {
                float *out = (float *)dst;
                for (ma_uint32 i = 0; i < n; i++)
                        out[i] = (float)src[i] / 32768.0f;
        }
}

static void alac_pcm24_to_output(const uint8_t *src, void *dst, ma_uint32 frames,
                                 ma_uint32 channels, ma_format outFmt)
{
        ma_uint32 n = frames * channels;
        if (outFmt == ma_format_s16) {
                int16_t *out = (int16_t *)dst;
                for (ma_uint32 i = 0; i < n; i++) {
                        int32_t s = src[i * 3] | (src[i * 3 + 1] << 8) | (src[i * 3 + 2] << 16);
                        if (s & 0x800000)
                                s |= 0xFF000000;
                        out[i] = (int16_t)(s >> 8);
                }
        } else {
                float *out = (float *)dst;
                for (ma_uint32 i = 0; i < n; i++) {
                        int32_t s = src[i * 3] | (src[i * 3 + 1] << 8) | (src[i * 3 + 2] << 16);
                        if (s & 0x800000)
                                s |= 0xFF000000;
                        out[i] = (float)s / 8388608.0f;
                }
        }
}

MA_API ma_result m4a_decoder_read_pcm_frames(
    m4a_decoder *pM4a,
    void *p_frames_out,
    ma_uint64 frame_count,
    ma_uint64 *p_frames_read)
{
        if (pM4a == NULL || p_frames_out == NULL || frame_count == 0) {
                return MA_INVALID_ARGS;
        }

        ma_result result = MA_SUCCESS;
        ma_uint32 channels = pM4a->channels;
        ma_uint32 sampleSize = pM4a->sampleSize;
        ma_uint64 totalFramesProcessed = 0;

        // Handle any leftover samples from previous call using the global/static leftover buffer
        if (pM4a->leftoverSampleCount > 0) {
                ma_uint64 leftoverToProcess = (pM4a->leftoverSampleCount < frame_count) ? pM4a->leftoverSampleCount : frame_count;
                ma_uint64 leftoverBytes = leftoverToProcess * channels * sampleSize;

                memcpy(p_frames_out, pM4a->leftoverBuffer, leftoverBytes);
                totalFramesProcessed += leftoverToProcess;

                // Shift the leftover buffer
                ma_uint64 samplesLeft = pM4a->leftoverSampleCount - leftoverToProcess;
                if (samplesLeft > 0) {
                        memmove(pM4a->leftoverBuffer, pM4a->leftoverBuffer + leftoverBytes, samplesLeft * channels * sampleSize);
                }
                pM4a->leftoverSampleCount = samplesLeft;
        }

        while (totalFramesProcessed < frame_count) {
                if (pM4a->file_type == k_rawAAC) {
                        /*
                        * ADTS frames have either a 7-byte header or a 9-byte header when
                        * CRC protection is present.
                        *
                        * We need at least 7 bytes to determine the frame length and whether
                        * the CRC field is present.
                        */
                        uint8_t adts_header[7];

                        size_t header_bytes_read = fread(
                            adts_header,
                            1,
                            sizeof(adts_header),
                            pM4a->file);

                        if (header_bytes_read == 0) {
                                /*
                                * Clean EOF.
                                */
                                result = MA_AT_END;
                                break;
                        }

                        if (header_bytes_read != sizeof(adts_header)) {
                                /*
                                * A partial ADTS header means a truncated/corrupt file.
                                */
                                result = MA_ERROR;
                                break;
                        }

                        /*
                        * Validate the ADTS sync word.
                        *
                        * syncword = 0xFFF (12 bits)
                        */
                        if (adts_header[0] != 0xFF ||
                            (adts_header[1] & 0xF0) != 0xF0) {

                                result = MA_ERROR;
                                break;
                        }

                        /*
                        * This decoder expects MPEG-4 AAC, not MPEG-2 AAC.
                        *
                        * ID:
                        *   0 = MPEG-4
                        *   1 = MPEG-2
                        *
                        * If your application intentionally supports MPEG-2 ADTS as well,
                        * this check can be removed.
                        */
                        if ((adts_header[1] & 0x08) != 0) {
                                result = MA_ERROR;
                                break;
                        }

                        /*
                        * protection_absent:
                        *
                        *   1 = no CRC, 7-byte header
                        *   0 = CRC present, 9-byte header
                        */
                        size_t adts_header_size =
                            (adts_header[1] & 0x01) != 0 ? 7u : 9u;

                        /*
                        * AAC frame length is a 13-bit field:
                        *
                        *   adts_header[3] bits 1..0
                        *   adts_header[4] bits 7..0
                        *   adts_header[5] bits 7..5
                        *
                        * It includes the ADTS header itself.
                        */
                        size_t frame_size =
                            ((size_t)(adts_header[3] & 0x03) << 11) |
                            ((size_t)adts_header[4] << 3) |
                            ((size_t)(adts_header[5] & 0xE0) >> 5);

                        /*
                        * Validate the frame size before doing any arithmetic based on it.
                        */
                        if (frame_size < adts_header_size) {
                                result = MA_ERROR;
                                break;
                        }

                        /*
                        * The ADTS frame length is 13 bits, so the theoretical maximum is
                        * 8191 bytes.
                        *
                        * Keep the explicit upper bound as a sanity check.
                        */
                        if (frame_size > 8191) {
                                result = MA_ERROR;
                                break;
                        }

                        /*
                        * payload_size is the number of bytes passed to FAAD2.
                        *
                        * This is the variable you were asking about earlier.
                        */
                        size_t payload_size = frame_size - adts_header_size;

                        /*
                        * An empty AAC payload isn't a valid decodable frame.
                        */
                        if (payload_size == 0) {
                                result = MA_ERROR;
                                break;
                        }

                        /*
                        * We need storage for the complete ADTS frame because the existing
                        * decoder buffer is used for the encoded AAC data.
                        *
                        * If pM4a->buffer is not guaranteed to exist for raw AAC, allocate
                        * it here.
                        */
                        if (pM4a->buffer == NULL || pM4a->buffer_size < frame_size) {
                                void *new_buffer = realloc(pM4a->buffer, frame_size);

                                if (new_buffer == NULL) {
                                        result = MA_OUT_OF_MEMORY;
                                        break;
                                }

                                pM4a->buffer = new_buffer;
                                pM4a->buffer_size = frame_size;
                        }

                        /*
                        * We already consumed the first 7 bytes of the header.
                        */
                        memcpy(pM4a->buffer, adts_header, sizeof(adts_header));

                        /*
                        * If CRC protection is present, read the additional two header bytes.
                        */
                        if (adts_header_size == 9) {
                                size_t crc_bytes_read =
                                    fread(
                                        (uint8_t *)pM4a->buffer + 7,
                                        1,
                                        2,
                                        pM4a->file);

                                if (crc_bytes_read != 2) {
                                        result = MA_ERROR;
                                        break;
                                }
                        }

                        /*
                        * Read the AAC payload.
                        */
                        size_t payload_bytes_read =
                            fread(
                                (uint8_t *)pM4a->buffer + adts_header_size,
                                1,
                                payload_size,
                                pM4a->file);

                        if (payload_bytes_read != payload_size) {
                                /*
                                * The file ended in the middle of an ADTS frame.
                                */
                                result = MA_ERROR;
                                break;
                        }

                        /*
                        * Decode only the AAC payload.
                        *
                        * Do NOT pass the ADTS header to NeAACDecDecode().
                        */
                        void *decodedData =
                            NeAACDecDecode(
                                pM4a->hDecoder,
                                &pM4a->frameInfo,
                                (uint8_t *)pM4a->buffer + adts_header_size,
                                (unsigned long)payload_size);

                        pM4a->current_sample++;

                        if (pM4a->frameInfo.error != 0) {
                                k_log(
                                    "ADTS AAC decoding error %d: %s\n",
                                    pM4a->frameInfo.error,
                                    NeAACDecGetErrorMessage(pM4a->frameInfo.error));

                                /*
                                * Do not return success just because an invalid frame was consumed.
                                * Continue trying subsequent ADTS frames.
                                */
                                continue;
                        }

                        if (decodedData == NULL || pM4a->frameInfo.samples == 0) {
                                /*
                                * FAAD2 produced no PCM.
                                */
                                continue;
                        }

                        /*
                        * HE-AAC/SBR/PS are intentionally unsupported by this decoder.
                        */
                        if (pM4a->frameInfo.sbr || pM4a->frameInfo.ps) {
                                k_log("HE-AAC/SBR/PS is not supported.\n");
                                set_error_message(
                                    "The current or next file is encoded with HE-AAC which is not supported.");

                                result = MA_ERROR;
                                break;
                        }

                        /*
                        * FAAD2 reports samples across all channels.
                        *
                        * Example:
                        *
                        *   1024 samples * 2 channels = 2048
                        *
                        * Therefore:
                        *
                        *   frames = samples / channels
                        */
                        ma_uint64 framesDecoded =
                            (ma_uint64)pM4a->frameInfo.samples / pM4a->channels;

                        if (framesDecoded == 0) {
                                continue;
                        }

                        ma_uint64 framesNeeded =
                            frame_count - totalFramesProcessed;

                        ma_uint64 framesToCopy =
                            framesDecoded < framesNeeded
                                ? framesDecoded
                                : framesNeeded;

                        ma_uint64 bytesToCopy =
                            framesToCopy *
                            (ma_uint64)pM4a->channels *
                            (ma_uint64)pM4a->sampleSize;

                        memcpy(
                            (uint8_t *)p_frames_out +
                                totalFramesProcessed *
                                    (ma_uint64)pM4a->channels *
                                    (ma_uint64)pM4a->sampleSize,
                            decodedData,
                            (size_t)bytesToCopy);

                        totalFramesProcessed += framesToCopy;

                        /*
                        * If FAAD2 decoded more frames than the caller requested, retain the
                        * remaining frames for the next read_pcm_frames() call.
                        */
                        if (framesToCopy < framesDecoded) {
                                ma_uint64 leftoverFrames =
                                    framesDecoded - framesToCopy;

                                ma_uint64 leftoverBytes =
                                    leftoverFrames *
                                    (ma_uint64)pM4a->channels *
                                    (ma_uint64)pM4a->sampleSize;

                                ma_uint64 maxLeftoverBytes =
                                    sizeof(pM4a->leftoverBuffer);

                                if (leftoverBytes > maxLeftoverBytes) {
                                        leftoverBytes = maxLeftoverBytes;

                                        leftoverFrames =
                                            leftoverBytes /
                                            ((ma_uint64)pM4a->channels *
                                             (ma_uint64)pM4a->sampleSize);

                                        leftoverBytes =
                                            leftoverFrames *
                                            (ma_uint64)pM4a->channels *
                                            (ma_uint64)pM4a->sampleSize;
                                }

                                memcpy(
                                    pM4a->leftoverBuffer,
                                    (uint8_t *)decodedData + bytesToCopy,
                                    (size_t)leftoverBytes);

                                pM4a->leftoverSampleCount = leftoverFrames;
                        } else {
                                pM4a->leftoverSampleCount = 0;
                        }
                } else if (pM4a->file_type == k_ALAC) {
                        if (pM4a->current_sample >= pM4a->total_samples) {
                                result = MA_AT_END;
                                break;
                        }

                        struct mp4_track_sample sample;
                        int ret = mp4_demux_get_track_sample(
                            pM4a->mp4, pM4a->audio_track_id, pM4a->current_sample,
                            pM4a->buffer, pM4a->buffer_size, NULL, 0, &sample);

                        if (ret < 0) {
                                result = MA_ERROR;
                                break;
                        }

                        pM4a->current_sample++;

                        int outSize = (int)pM4a->alacScratchSize;
                        alac_decode_frame(pM4a->alac, pM4a->buffer, pM4a->alacScratch, &outSize);

                        if (outSize <= 0) {
                                continue; // decode failed for this frame, try the next
                        }

                        ma_uint32 bytesPerSrcFrame = pM4a->channels * (pM4a->alac->setinfo_sample_size / 8);
                        ma_uint64 framesDecoded = outSize / bytesPerSrcFrame;

                        // Convert into a temp buffer at output format/rate, same leftover-buffer
                        // pattern you already use for AAC. Reuse your existing leftoverBuffer here.
                        ma_uint64 framesNeeded = frame_count - totalFramesProcessed;
                        ma_uint64 framesToCopy = (framesDecoded < framesNeeded) ? framesDecoded : framesNeeded;

                        if (pM4a->alac->setinfo_sample_size == 16) {
                                alac_pcm16_to_output((const int16_t *)pM4a->alacScratch,
                                                     (uint8_t *)p_frames_out + totalFramesProcessed * channels * sampleSize,
                                                     (ma_uint32)framesToCopy, channels, pM4a->format);
                        } else { // 24-bit
                                alac_pcm24_to_output(pM4a->alacScratch,
                                                     (uint8_t *)p_frames_out + totalFramesProcessed * channels * sampleSize,
                                                     (ma_uint32)framesToCopy, channels, pM4a->format);
                        }
                        totalFramesProcessed += framesToCopy;

                        if (framesToCopy < framesDecoded) {
                                ma_uint64 leftoverFrames = framesDecoded - framesToCopy;
                                ma_uint64 leftoverBytes = leftoverFrames * channels * sampleSize;

                                if (leftoverBytes > sizeof(pM4a->leftoverBuffer)) {
                                        leftoverFrames = sizeof(pM4a->leftoverBuffer) / (channels * sampleSize);
                                        leftoverBytes = leftoverFrames * channels * sampleSize;
                                }

                                const uint8_t *tailSrc = pM4a->alacScratch + (size_t)framesToCopy * bytesPerSrcFrame;

                                if (pM4a->alac->setinfo_sample_size == 16) {
                                        alac_pcm16_to_output((const int16_t *)tailSrc, pM4a->leftoverBuffer,
                                                             (ma_uint32)leftoverFrames, channels, pM4a->format);
                                } else {
                                        alac_pcm24_to_output(tailSrc, pM4a->leftoverBuffer,
                                                             (ma_uint32)leftoverFrames, channels, pM4a->format);
                                }

                                pM4a->leftoverSampleCount = leftoverFrames;
                        } else {
                                pM4a->leftoverSampleCount = 0;
                        }
                }

                else {
                        if (pM4a->current_sample >= pM4a->total_samples) {
                                result = MA_AT_END;
                                break; // No more samples
                        }

                        unsigned int frame_bytes = 0;

                        // Get the sample offset and size
                        struct mp4_track_sample sample;

                        int ret =
                            mp4_demux_get_track_sample(
                                pM4a->mp4,
                                pM4a->audio_track_id,
                                pM4a->current_sample,
                                pM4a->buffer,
                                pM4a->buffer_size,
                                NULL,
                                0,
                                &sample);

                        if (ret < 0) {
                                // Error getting sample info
                                result = MA_ERROR;
                                break;
                        }

                        frame_bytes = sample.size;

                        pM4a->current_sample++;

                        void *decodedData =
                            NeAACDecDecode(
                                pM4a->hDecoder,
                                &pM4a->frameInfo,
                                pM4a->buffer,
                                frame_bytes);

                        if (pM4a->frameInfo.error > 0) {
                                k_log("Decoding Error %d: %s\n",
                                      pM4a->frameInfo.error,
                                      NeAACDecGetErrorMessage(pM4a->frameInfo.error));
                                continue;
                        }

                        // Remove support for HE-AAC components (SBR or PS)
                        if (pM4a->frameInfo.sbr || pM4a->frameInfo.ps) {
                                // HE - AAC detected(either SBR or PS is present), skip processing continue;
                                continue;
                        }

                        unsigned long samplesDecoded = pM4a->frameInfo.samples; // Total samples decoded (channels * frames)
                        ma_uint64 framesDecoded = samplesDecoded / channels;

                        // Calculate how many frames we can process in this call
                        ma_uint64 framesNeeded = frame_count - totalFramesProcessed;
                        ma_uint64 frames_to_copy = (framesDecoded < framesNeeded) ? framesDecoded : framesNeeded;
                        ma_uint64 bytesToCopy = frames_to_copy * channels * sampleSize;

                        memcpy((uint8_t *)p_frames_out + totalFramesProcessed * channels * sampleSize, decodedData, bytesToCopy);
                        totalFramesProcessed += frames_to_copy;

                        // Handle leftover frames using the global/static leftover buffer
                        if (frames_to_copy < framesDecoded) {
                                // There are leftover frames
                                pM4a->leftoverSampleCount = framesDecoded - frames_to_copy;
                                ma_uint64 leftoverBytes = pM4a->leftoverSampleCount * channels * sampleSize;

                                if (leftoverBytes > sizeof(pM4a->leftoverBuffer)) {
                                        // Safety check to avoid overflow in the buffer.
                                        pM4a->leftoverSampleCount = sizeof(pM4a->leftoverBuffer) / (channels * sampleSize);
                                        leftoverBytes = pM4a->leftoverSampleCount * channels * sampleSize;
                                }

                                memcpy(pM4a->leftoverBuffer, (uint8_t *)decodedData + bytesToCopy, leftoverBytes);
                        } else {
                                pM4a->leftoverSampleCount = 0;
                        }
                }
        }

        pM4a->cursor += totalFramesProcessed;

        if (p_frames_read != NULL) {
                *p_frames_read = totalFramesProcessed;
        }

        if (totalFramesProcessed > 0) {
                return MA_SUCCESS;
        }

        return result;
}

MA_API ma_result m4a_decoder_seek_to_pcm_frame(m4a_decoder *pM4a, ma_uint64 frame_index)
{
        if (pM4a == NULL)
                return MA_INVALID_ARGS;

        if (pM4a->file_type == k_rawAAC) {
                if (pM4a->adts_frame_count == 0)
                        return MA_INVALID_OPERATION;

                uint32_t low = 0, high = pM4a->adts_frame_count;
                while (low < high) {
                        uint32_t mid = low + (high - low) / 2;
                        if (pM4a->adts_pcm_offsets[mid] <= frame_index)
                                low = mid + 1;
                        else
                                high = mid;
                }

                uint32_t index = low ? low - 1 : 0;

                if (pM4a->adts_frame_offsets[index] > (uint64_t)INT64_MAX ||
                    fseeko(pM4a->file,
                           (off_t)pM4a->adts_frame_offsets[index],
                           SEEK_SET) != 0)
                        return MA_ERROR;

                pM4a->current_sample = index;
                pM4a->cursor = pM4a->adts_pcm_offsets[index];
                pM4a->leftoverSampleCount = 0;

                NeAACDecPostSeekReset(pM4a->hDecoder, (long)index);

                return MA_SUCCESS;

        } else if (pM4a->file_type == k_ALAC) {

                ma_uint32 samplesPerFrame = pM4a->alac->setinfo_max_samples_per_frame;

                if (frame_index >= (ma_uint64)pM4a->total_samples * samplesPerFrame)
                        return MA_INVALID_ARGS;

                pM4a->current_sample = (ma_uint32)(frame_index / samplesPerFrame);

                if (pM4a->sample_rate == 0)
                        return MA_ERROR;

                uint64_t time_us =
                    ((uint64_t)pM4a->current_sample * samplesPerFrame * 1000000ULL) / pM4a->sample_rate;

                int seek_ret = mp4_demux_seek(
                    pM4a->mp4,
                    time_us,
                    MP4_SEEK_METHOD_PREVIOUS_SYNC);

                if (seek_ret < 0)
                        return MA_ERROR;

                struct mp4_track_sample sample;
                int ret = mp4_demux_get_track_sample(
                    pM4a->mp4, pM4a->audio_track_id, pM4a->current_sample,
                    NULL, 0, NULL, 0, &sample);

                k_log("ALAC seek: frame_index=%llu target_sample=%u time_us=%llu ret=%d\n",
                      (unsigned long long)frame_index, pM4a->current_sample,
                      (unsigned long long)time_us, ret);

                if (ret < 0)
                        return MA_ERROR;

                pM4a->leftoverSampleCount = 0;

                uint64_t actual_pcm_frame =
                    (sample.dts * (uint64_t)pM4a->sample_rate) / pM4a->track.timescale;

                pM4a->current_sample = (ma_uint32)(actual_pcm_frame / samplesPerFrame);
                pM4a->cursor = actual_pcm_frame;

                k_log("ALAC seek result: dts=%llu actual_pcm_frame=%llu final_sample=%u\n",
                      (unsigned long long)sample.dts, (unsigned long long)actual_pcm_frame,
                      pM4a->current_sample);

                return MA_SUCCESS;
        } else {

                if (frame_index >= pM4a->total_samples * 1024)
                        return MA_INVALID_ARGS;

                pM4a->current_sample = (ma_uint32)(frame_index / 1024);

                unsigned int frame_bytes = 0;
                struct mp4_track_sample sample;

                // frame_index -> PCM frame count -> microseconds
                // time_us = frames / sample_rate * 1,000,000
                uint64_t time_us =
                    ((uint64_t)pM4a->current_sample * 1024ULL * 1000000ULL) / pM4a->sample_rate;

                int seek_ret = mp4_demux_seek(
                    pM4a->mp4,
                    time_us,
                    MP4_SEEK_METHOD_PREVIOUS_SYNC);

                if (seek_ret < 0)
                        return MA_ERROR;

                int ret =
                    mp4_demux_get_track_sample(
                        pM4a->mp4,
                        pM4a->audio_track_id,
                        pM4a->current_sample,
                        NULL,
                        0,
                        NULL,
                        0,
                        &sample);

                if (ret < 0)
                        return MA_ERROR;

                frame_bytes = sample.size;
                ma_int64 sample_offset = sample.offset;

                if (sample_offset < 0 || frame_bytes == 0) {
                        return MA_ERROR;
                }

                uint64_t actual_pcm_frame =
                    (sample.dts * (uint64_t)pM4a->sample_rate) / pM4a->track.timescale;

                pM4a->current_sample = (ma_uint32)(actual_pcm_frame / 1024);

                NeAACDecPostSeekReset(pM4a->hDecoder, (long)pM4a->current_sample);

                pM4a->leftoverSampleCount = 0;
                pM4a->cursor = actual_pcm_frame;

                return MA_SUCCESS;
        }
}

MA_API ma_result m4a_decoder_get_data_format(
    m4a_decoder *pM4a,
    ma_format *p_format,
    ma_uint32 *p_channels,
    ma_uint32 *p_sample_rate,
    ma_channel *p_channel_map,
    size_t channel_map_cap)
{
        // Initialize output variables
        if (p_format != NULL) {
                *p_format = ma_format_unknown;
        }
        if (p_channels != NULL) {
                *p_channels = 0;
        }
        if (p_sample_rate != NULL) {
                *p_sample_rate = 0;
        }
        if (p_channel_map != NULL) {
                MA_ZERO_MEMORY(p_channel_map, sizeof(*p_channel_map) * channel_map_cap);
        }

        if (pM4a == NULL) {
                return MA_INVALID_OPERATION;
        }

        if (p_format != NULL) {
                *p_format = pM4a->format;
        }

        if (p_channels != NULL) {
                *p_channels = pM4a->channels;
        }

        if (p_sample_rate != NULL) {
                *p_sample_rate = pM4a->sample_rate;
        }

        // Set a standard channel map if requested
        if (p_channel_map != NULL) {
                ma_channel_map_init_standard(ma_standard_channel_map_microsoft, p_channel_map, channel_map_cap, *p_channels);
        }

        return MA_SUCCESS;
}

MA_API ma_result m4a_decoder_get_cursor_in_pcm_frames(m4a_decoder *pM4a, ma_uint64 *p_cursor)
{
        if (p_cursor == NULL) {
                return MA_INVALID_ARGS;
        }

        *p_cursor = 0; /* Safety. */

        if (pM4a == NULL) {
                return MA_INVALID_ARGS;
        }

        *p_cursor = pM4a->cursor;

        return MA_SUCCESS;
}

MA_API ma_result m4a_decoder_get_length_in_pcm_frames(m4a_decoder *pM4a,
                                                      ma_uint64 *p_length)
{
        if (p_length == NULL) {
                return MA_INVALID_ARGS;
        }

        *p_length = 0;

        if (pM4a == NULL) {
                return MA_INVALID_ARGS;
        }

        if (pM4a->sample_rate == 0) {
                return MA_ERROR;
        }

        if (pM4a->file_type == k_rawAAC) {
                *p_length = pM4a->adts_total_pcm_frames;
                return *p_length ? MA_SUCCESS : MA_ERROR;
        }

        if (pM4a->track.timescale == 0)
                return MA_ERROR;

        *p_length =
            ((ma_uint64)pM4a->track.duration * pM4a->sample_rate) /
            pM4a->track.timescale;

        return MA_SUCCESS;
}
#endif
#endif
#ifdef __cplusplus
}
#endif
#endif
