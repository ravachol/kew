/**
 * @file webm.h
 * @brief WebM audio decoding interface.
 *
 * Declares functions and types for decoding WebM/Opus audio files.
 */

#ifndef WEBM_H
#define WEBM_H

#ifdef __cplusplus

extern "C" {
#endif

#include "miniaudio.h"

#if !defined(MA_NO_WEBM)

#include <nestegg/nestegg.h>
#include <opusfile.h>
#include <vorbis/codec.h>

#endif

typedef struct
{
        ma_data_source_base ds; /* The webm decoder can be used independently as a data source. */
        ma_read_proc onRead;
        ma_seek_proc onSeek;
        ma_tell_proc onTell;
        void *pReadSeekTellUserData;
        ma_format format; /* Will be f32 */
#if !defined(MA_NO_WEBM)
        ma_uint64 audio_track;
        ma_uint64 cursorInPCMFrames;
        ma_uint64 seekTargetPCMFrame;
        ma_uint32 sample_rate;
        ma_uint32 channels;
        ma_uint64 lengthInPCMFrames;
        double duration;

        // Nestegg fields
        nestegg *ctx;
        unsigned int codec_id;
        nestegg_packet *currentPacket;
        unsigned int numFramesInPacket;
        unsigned int currentPacketFrame;
        ma_bool32 hasPacket;

        // Opus fields
        OpusDecoder *opusDecoder;
        // Vorbis fields
        vorbis_block vorbisBlock;
        vorbis_dsp_state vorbisDSP;
        vorbis_comment vorbisComment;
        vorbis_info vorbisInfo;
        ma_uint16 opusPreSkip;
        ma_uint16 preSkipLeft;

        ma_uint64 bufferLeftoverFrameCount;
        ma_uint64 bufferLeftoverFrameOffset;

#endif
} ma_webm;

/**
 * @brief Initializes a WebM decoder using custom I/O callbacks.
 *
 * Creates and initializes a WebM decoding context using user-provided
 * read/seek/tell callbacks. Supports Opus and Vorbis audio tracks.
 *
 * @param onRead                    Callback for reading data.
 * @param onSeek                    Callback for seeking within the stream.
 * @param onTell                    Callback for retrieving the current position (optional).
 * @param pReadSeekTellUserData     User data passed to I/O callbacks.
 * @param p_config                  Optional decoding backend configuration.
 * @param p_allocation_callbacks    Optional allocation callbacks (currently unused).
 * @param p_webm                    Pointer to the WebM decoder to initialize.
 *
 * @return MA_SUCCESS on success.
 * @return MA_INVALID_ARGS if required arguments are NULL.
 * @return MA_INVALID_FILE if the file is not a valid WebM audio file.
 * @return MA_NOT_IMPLEMENTED if WebM support is disabled or codec unsupported.
 */
MA_API ma_result ma_webm_init(ma_read_proc onRead,
                              ma_seek_proc onSeek,
                              ma_tell_proc onTell,
                              void *pReadSeekTellUserData,
                              const ma_decoding_backend_config *p_config,
                              const ma_allocation_callbacks *p_allocation_callbacks,
                              ma_webm *p_webm);

/**
 * @brief Initializes a WebM decoder from a file on disk.
 *
 * Opens the specified file path and initializes decoding for the first
 * audio track found (Opus or Vorbis).
 *
 * @param pFilePath                 Path to the WebM file.
 * @param p_config                  Optional decoding backend configuration.
 * @param p_allocation_callbacks    Optional allocation callbacks (currently unused).
 * @param p_webm                    Pointer to the WebM decoder to initialize.
 *
 * @return MA_SUCCESS on success.
 * @return MA_INVALID_FILE if the file cannot be opened or parsed.
 * @return MA_NOT_IMPLEMENTED if WebM support is disabled or codec unsupported.
 */
MA_API ma_result ma_webm_init_file(const char *pFilePath,
                                   const ma_decoding_backend_config *p_config,
                                   const ma_allocation_callbacks *p_allocation_callbacks,
                                   ma_webm *p_webm);

/**
 * @brief Uninitializes a WebM decoder and releases associated resources.
 *
 * Frees codec-specific decoder state (Opus/Vorbis), destroys the
 * underlying container context, and uninitializes the data source.
 *
 * @param p_webm                 Pointer to the WebM decoder.
 * @param p_allocation_callbacks Optional allocation callbacks (currently unused).
 */
MA_API void ma_webm_uninit(ma_webm *p_webm,
                           const ma_allocation_callbacks *p_allocation_callbacks);

/**
 * @brief Reads decoded PCM frames from the WebM stream.
 *
 * Decodes audio packets and writes interleaved floating-point PCM frames
 * to the output buffer. Handles internal buffering, pre-skip (Opus),
 * and seek discard logic.
 *
 * @param p_webm        Pointer to the WebM decoder.
 * @param p_frames_out  Output buffer for decoded PCM frames (f32 format).
 * @param frame_count   Number of PCM frames to read.
 * @param p_frames_read Optional pointer to receive the number of frames read.
 *
 * @return MA_SUCCESS on success.
 * @return MA_AT_END if the end of stream is reached.
 * @return MA_INVALID_ARGS if arguments are invalid.
 * @return MA_ERROR on decoding failure.
 */
MA_API ma_result ma_webm_read_pcm_frames(ma_webm *p_webm,
                                         void *p_frames_out,
                                         ma_uint64 frame_count,
                                         ma_uint64 *p_frames_read);

/**
 * @brief Seeks to a specific PCM frame in the stream.
 *
 * Performs a container-level seek and resets decoder state.
 * For Opus streams, applies preroll and pre-skip handling
 * as required by the specification.
 *
 * @param p_webm       Pointer to the WebM decoder.
 * @param frame_index  Target PCM frame index.
 *
 * @return MA_SUCCESS on success.
 * @return MA_INVALID_ARGS if arguments are invalid.
 * @return MA_INVALID_OPERATION if seeking fails.
 */
MA_API ma_result ma_webm_seek_to_pcm_frame(ma_webm *p_webm,
                                           ma_uint64 frame_index);

/**
 * @brief Retrieves the audio format of the decoded stream.
 *
 * Returns the sample format, channel count, sample rate,
 * and optionally fills a standard channel map.
 *
 * @param p_webm          Pointer to the WebM decoder.
 * @param p_format        Pointer to receive the sample format.
 * @param p_channels      Pointer to receive the number of channels.
 * @param p_sample_rate   Pointer to receive the sample rate.
 * @param p_channel_map   Optional buffer to receive the channel map.
 * @param channel_map_cap Capacity of the channel map buffer.
 *
 * @return MA_SUCCESS on success.
 * @return MA_INVALID_OPERATION if decoder is invalid.
 */
MA_API ma_result ma_webm_get_data_format(ma_webm *p_webm,
                                         ma_format *p_format,
                                         ma_uint32 *p_channels,
                                         ma_uint32 *p_sample_rate,
                                         ma_channel *p_channel_map,
                                         size_t channel_map_cap);

/**
 * @brief Retrieves the current playback cursor position in PCM frames.
 *
 * @param p_webm    Pointer to the WebM decoder.
 * @param p_cursor  Pointer to receive the current PCM frame index.
 *
 * @return MA_SUCCESS on success.
 * @return MA_INVALID_ARGS if arguments are invalid.
 */
MA_API ma_result ma_webm_get_cursor_in_pcm_frames(ma_webm *p_webm,
                                                  ma_uint64 *p_cursor);

/**
 * @brief Retrieves the total length of the stream in PCM frames.
 *
 * Computes and caches the total frame count based on container duration.
 * For Opus streams, pre-skip is subtracted from the total frame count.
 *
 * @param p_webm    Pointer to the WebM decoder.
 * @param p_length  Pointer to receive the total PCM frame length.
 *
 * @return MA_SUCCESS on success.
 * @return MA_INVALID_ARGS if arguments are invalid.
 */
MA_API ma_result ma_webm_get_length_in_pcm_frames(ma_webm *p_webm,
                                                  ma_uint64 *p_length);


#ifdef __cplusplus
}
#endif
#endif

#if defined(MINIAUDIO_IMPLEMENTATION) || defined(MA_IMPLEMENTATION)

#define MAX_OPUS_CHANNELS 8
#define MAX_OPUS_SAMPLES 5760 // Maximum expected frame size

static float opusLeftoverBuffer[MAX_OPUS_SAMPLES * MAX_OPUS_CHANNELS];

#define MAX_VORBIS_PACKET_FRAMES 4096
#define MAX_VORBIS_CHANNELS 8

float vorbisLeftoverBuffer[MAX_VORBIS_PACKET_FRAMES * MAX_VORBIS_CHANNELS];

static ma_result ma_webm_ds_read(ma_data_source *p_data_source, void *p_frames_out, ma_uint64 frame_count, ma_uint64 *p_frames_read)
{
        return ma_webm_read_pcm_frames((ma_webm *)p_data_source, p_frames_out, frame_count, p_frames_read);
}

static ma_result ma_webm_ds_seek(ma_data_source *p_data_source, ma_uint64 frame_index)
{
        return ma_webm_seek_to_pcm_frame((ma_webm *)p_data_source, frame_index);
}

static ma_result ma_webm_ds_get_data_format(ma_data_source *p_data_source, ma_format *p_format, ma_uint32 *p_channels, ma_uint32 *p_sample_rate, ma_channel *p_channel_map, size_t channel_map_cap)
{
        return ma_webm_get_data_format((ma_webm *)p_data_source, p_format, p_channels, p_sample_rate, p_channel_map, channel_map_cap);
}

static ma_result ma_webm_ds_get_cursor(ma_data_source *p_data_source, ma_uint64 *p_cursor)
{
        return ma_webm_get_cursor_in_pcm_frames((ma_webm *)p_data_source, p_cursor);
}

static ma_result ma_webm_ds_get_length(ma_data_source *p_data_source, ma_uint64 *p_length)
{
        return ma_webm_get_length_in_pcm_frames((ma_webm *)p_data_source, p_length);
}

static ma_data_source_vtable g_ma_webm_ds_vtable =
    {
        ma_webm_ds_read,
        ma_webm_ds_seek,
        ma_webm_ds_get_data_format,
        ma_webm_ds_get_cursor,
        ma_webm_ds_get_length,
        NULL,
        (ma_uint64)0};

static ma_result ma_webm_init_internal(const ma_decoding_backend_config *p_config, ma_webm *p_webm)
{
        ma_result result;
        ma_data_source_config dataSourceConfig;

        if (p_webm == NULL) {
                return MA_INVALID_ARGS;
        }

        MA_ZERO_OBJECT(p_webm);
        p_webm->format = ma_format_f32; // f32 by default.
        p_webm->seekTargetPCMFrame = (ma_uint64)-1;

        // Clear leftover buffer
        p_webm->bufferLeftoverFrameCount = 0;
        p_webm->bufferLeftoverFrameOffset = 0;

        if (p_config != NULL && (p_config->preferredFormat == ma_format_f32 || p_config->preferredFormat == ma_format_s16)) {
                p_webm->format = p_config->preferredFormat;
        } else {
                /* Getting here means something other than f32 and s16 was specified. Just leave this unset to use the default format. */
        }

        dataSourceConfig = ma_data_source_config_init();
        dataSourceConfig.vtable = &g_ma_webm_ds_vtable;

        result = ma_data_source_init(&dataSourceConfig, &p_webm->ds);
        if (result != MA_SUCCESS) {
                return result; /* Failed to initialize the base data source. */
        }

        return MA_SUCCESS;
}

static int nestegg_io_read(void *buffer, size_t length, void *userdata)
{
        ma_webm *webm = (ma_webm *)userdata;
        size_t bytes_read = 0;
        if (webm->onRead(webm->pReadSeekTellUserData, buffer, length, &bytes_read) == MA_SUCCESS)
                return (bytes_read == length) ? 1 : 0;
        return -1;
}

static int nestegg_io_seek(int64_t offset, int whence, void *userdata)
{
        ma_webm *webm = (ma_webm *)userdata;
        ma_seek_origin origin;
        switch (whence) {
        case NESTEGG_SEEK_SET:
                origin = ma_seek_origin_start;
                break;
        case NESTEGG_SEEK_CUR:
                origin = ma_seek_origin_current;
                break;
        case NESTEGG_SEEK_END:
                origin = ma_seek_origin_end;
                break;
        default:
                return -1;
        }
        return (webm->onSeek(webm->pReadSeekTellUserData, offset, origin) == MA_SUCCESS) ? 0 : -1;
}

static int64_t nestegg_io_tell(void *userdata)
{
        ma_webm *webm = (ma_webm *)userdata;
        ma_int64 pos = 0;
        return (webm->onTell(webm->pReadSeekTellUserData, &pos) == MA_SUCCESS) ? pos : -1;
}

double calcWebmDuration(nestegg *ctx)
{
        double duration = 0.0f;

        uint64_t duration_ns = 0;
        if (nestegg_duration(ctx, &duration_ns) == 0) {
                duration = (double)duration_ns / 1e9;
        }
        return duration;
}

static int ma_webm_init_vorbis_decoder(
    nestegg *ctx,
    unsigned int audio_track,
    ma_webm *p_webm)
{
        unsigned char *id = NULL, *comment = NULL, *setup = NULL;
        size_t id_size = 0, comment_size = 0, setup_size = 0;
        ogg_packet header_packet;

        // Fetch header packets as delivered by WebM/Matroska.
        if (nestegg_track_codec_data(ctx, audio_track, 0, &id, &id_size) != 0 ||
            nestegg_track_codec_data(ctx, audio_track, 1, &comment, &comment_size) != 0 ||
            nestegg_track_codec_data(ctx, audio_track, 2, &setup, &setup_size) != 0) {
                return -1; // invalid file or track
        }

        // Setup libvorbis structures.
        vorbis_info_init(&p_webm->vorbisInfo);
        vorbis_comment_init(&p_webm->vorbisComment);

        memset(&header_packet, 0, sizeof(header_packet));
        // Header 1: ID
        header_packet.packet = id;
        header_packet.bytes = id_size;
        header_packet.b_o_s = 1;
        header_packet.e_o_s = 0;
        if (vorbis_synthesis_headerin(&p_webm->vorbisInfo, &p_webm->vorbisComment, &header_packet) != 0)
                goto fail;

        // Header 2: COMMENT
        header_packet.packet = comment;
        header_packet.bytes = comment_size;
        header_packet.b_o_s = 0;
        // header_packet.e_o_s remains 0
        if (vorbis_synthesis_headerin(&p_webm->vorbisInfo, &p_webm->vorbisComment, &header_packet) != 0)
                goto fail;

        // Header 3: SETUP
        header_packet.packet = setup;
        header_packet.bytes = setup_size;
        // header_packet.b_o_s remains 0
        if (vorbis_synthesis_headerin(&p_webm->vorbisInfo, &p_webm->vorbisComment, &header_packet) != 0)
                goto fail;

        // Setup decoder
        if (vorbis_synthesis_init(&p_webm->vorbisDSP, &p_webm->vorbisInfo) != 0)
                goto fail;
        if (vorbis_block_init(&p_webm->vorbisDSP, &p_webm->vorbisBlock) != 0)
                goto fail;

        p_webm->channels = p_webm->vorbisInfo.channels;
        p_webm->sample_rate = p_webm->vorbisInfo.rate;
        p_webm->format = ma_format_f32;

        return 0; // success

fail:
        vorbis_block_clear(&p_webm->vorbisBlock);
        vorbis_dsp_clear(&p_webm->vorbisDSP);
        vorbis_comment_clear(&p_webm->vorbisComment);
        vorbis_info_clear(&p_webm->vorbisInfo);
        return -2; // error
}

MA_API ma_result ma_webm_init(
    ma_read_proc onRead,
    ma_seek_proc onSeek,
    ma_tell_proc onTell,
    void *pReadSeekTellUserData,
    const ma_decoding_backend_config *p_config,
    const ma_allocation_callbacks *p_allocation_callbacks,
    ma_webm *p_webm)
{
        ma_result result;

        (void)p_allocation_callbacks;

        result = ma_webm_init_internal(p_config, p_webm);
        if (result != MA_SUCCESS) {
                return result;
        }

        if (onRead == NULL || onSeek == NULL) {
                return MA_INVALID_ARGS; /* onRead and onSeek are mandatory. */
        }

        p_webm->onRead = onRead;
        p_webm->onSeek = onSeek;
        p_webm->onTell = onTell;
        p_webm->pReadSeekTellUserData = pReadSeekTellUserData;

#if !defined(MA_NO_WEBM)
        nestegg_io io = {0};

        // Adapter functions for nestegg
        io.read = nestegg_io_read;
        io.seek = nestegg_io_seek;
        io.tell = nestegg_io_tell;
        io.userdata = p_webm;

        nestegg *ctx = NULL;
        if (nestegg_init(&ctx, io, NULL, -1) < 0) {
                return MA_INVALID_FILE;
        }

        // Find Audio Track
        unsigned int num_tracks = 0;
        if (nestegg_track_count(ctx, &num_tracks) != 0) {
                nestegg_destroy(ctx);
                return MA_INVALID_FILE;
        }

        p_webm->audio_track = (unsigned int)-1;
        p_webm->codec_id = -1;
        for (unsigned int i = 0; i < num_tracks; ++i) {
                unsigned int type = 0;
                type = nestegg_track_type(ctx, i);
                if (type == NESTEGG_TRACK_AUDIO) {
                        p_webm->audio_track = i;
                        p_webm->codec_id = nestegg_track_codec_id(ctx, i);
                        if (p_webm->codec_id == 0) {
                                break; // first audio
                        }
                }
        }

        if (p_webm->audio_track == (unsigned int)-1) {
                nestegg_destroy(ctx);
                return MA_INVALID_FILE;
        }

        // Prepare decoder
        if (p_webm->codec_id == NESTEGG_CODEC_OPUS) {
                unsigned char *header = NULL;
                size_t header_size = 0;
                if (nestegg_track_codec_data(ctx, p_webm->audio_track, 0, &header, &header_size) != 0 ||
                    header_size < 19 || memcmp(header, "OpusHead", 8) != 0) {
                        nestegg_destroy(ctx);
                        return MA_INVALID_FILE;
                }
                p_webm->sample_rate = 48000;
                p_webm->channels = header[9];
                ma_uint16 preSkip = header[10] | (header[11] << 8); // Little-endian
                p_webm->opusPreSkip = preSkip;
                p_webm->preSkipLeft = preSkip;
                int opusErr = 0;
                p_webm->opusDecoder = opus_decoder_create(p_webm->sample_rate, p_webm->channels, &opusErr);
                if (!p_webm->opusDecoder) {
                        nestegg_destroy(ctx);
                        return MA_INVALID_FILE;
                }
                p_webm->format = ma_format_f32;
        } else if (p_webm->codec_id == NESTEGG_CODEC_VORBIS) {
                if (ma_webm_init_vorbis_decoder(ctx, p_webm->audio_track, p_webm) != 0) {
                        nestegg_destroy(ctx);
                        return MA_INVALID_FILE;
                }
        } else {
                nestegg_destroy(ctx);
                return MA_NOT_IMPLEMENTED;
        }

        p_webm->ctx = ctx;

        p_webm->duration = calcWebmDuration(ctx);
        p_webm->seekTargetPCMFrame = (ma_uint64)(-1);

        return MA_SUCCESS;

#else
        (void)pReadSeekTellUserData;
        (void)p_config;
        (void)p_webm;
        return MA_NOT_IMPLEMENTED;
#endif
}

int nread(void *buf, size_t len, void *ud)
{
        FILE *f = (FILE *)ud;
        size_t r = fread(buf, 1, len, f);
        if (r == len)
                return 1;
        if (feof(f))
                return 0;
        return -1;
}

int nseek(int64_t o, int w, void *ud)
{
        FILE *f = (FILE *)ud;
        int wh;
        switch (w) {
        case NESTEGG_SEEK_SET:
                wh = SEEK_SET;
                break;
        case NESTEGG_SEEK_CUR:
                wh = SEEK_CUR;
                break;
        case NESTEGG_SEEK_END:
                wh = SEEK_END;
                break;
        default:
                return -1;
        }
        return fseek(f, (long)o, wh);
}

int64_t ntell(void *ud)
{
        FILE *f = (FILE *)ud;
        return ftell(f);
}

MA_API ma_result ma_webm_init_file(const char *pFilePath, const ma_decoding_backend_config *p_config, const ma_allocation_callbacks *p_allocation_callbacks, ma_webm *p_webm)
{
        ma_result result;

        (void)p_allocation_callbacks;

        result = ma_webm_init_internal(p_config, p_webm);
        if (result != MA_SUCCESS) {
                return result;
        }

#if !defined(MA_NO_WEBM)
        FILE *fp = fopen(pFilePath, "rb");
        if (!fp)
                return MA_INVALID_FILE;

        nestegg_io io = {nread, nseek, ntell, fp};
        nestegg *ctx = NULL;

        if (nestegg_init(&ctx, io, NULL, -1) < 0) {
                fclose(fp);
                return MA_INVALID_FILE;
        }

        unsigned int num_tracks = 0;
        nestegg_track_count(ctx, &num_tracks);
        p_webm->audio_track = (unsigned int)(-1);
        p_webm->codec_id = -1;
        for (unsigned int i = 0; i < num_tracks; ++i) {
                unsigned int type = 0;
                type = nestegg_track_type(ctx, i);
                if (type == NESTEGG_TRACK_AUDIO) {
                        p_webm->audio_track = i;
                        p_webm->codec_id = nestegg_track_codec_id(ctx, i);
                        break;
                }
        }
        if (p_webm->audio_track == (unsigned int)(-1)) {
                nestegg_destroy(ctx);
                fclose(fp);
                return MA_ERROR;
        }

        // Fetch and handle header
        if (p_webm->codec_id == NESTEGG_CODEC_OPUS) {
                unsigned char *header = NULL;
                size_t header_size = 0;
                nestegg_track_codec_data(ctx, p_webm->audio_track, 0, &header, &header_size);
                if (header_size < 19 || memcmp(header, "OpusHead", 8) != 0) {
                        nestegg_destroy(ctx);
                        fclose(fp);
                        return MA_ERROR;
                }
                p_webm->channels = header[9];
                ma_uint16 preSkip = header[10] | (header[11] << 8); // Little-endian

                p_webm->opusPreSkip = preSkip;
                p_webm->preSkipLeft = preSkip;
                int opusErr = 0;

                p_webm->opusDecoder = opus_decoder_create(48000, p_webm->channels, &opusErr);
                if (!p_webm->opusDecoder) {
                        nestegg_destroy(ctx);
                        fclose(fp);
                        return MA_ERROR;
                }

                p_webm->format = ma_format_f32;
                p_webm->sample_rate = 48000;
        } else if (p_webm->codec_id == NESTEGG_CODEC_VORBIS) {
                if (ma_webm_init_vorbis_decoder(ctx, p_webm->audio_track, p_webm) != 0) {
                        nestegg_destroy(ctx);
                        return MA_INVALID_FILE;
                }
        } else {
                nestegg_destroy(ctx);
                fclose(fp);
                return MA_NOT_IMPLEMENTED;
        }

        p_webm->ctx = ctx;

        p_webm->duration = calcWebmDuration(ctx);
        p_webm->seekTargetPCMFrame = (ma_uint64)(-1);

        return MA_SUCCESS;

#else
        /* webm is disabled. */
        (void)pFilePath;
        return MA_NOT_IMPLEMENTED;
#endif
}

MA_API void ma_webm_uninit(ma_webm *p_webm, const ma_allocation_callbacks *p_allocation_callbacks)
{
        if (p_webm == NULL) {
                return;
        }

        (void)p_allocation_callbacks;

#if !defined(MA_NO_WEBM)
        {
                if (p_webm->codec_id == NESTEGG_CODEC_OPUS) {
                        opus_decoder_destroy(p_webm->opusDecoder);
                        p_webm->opusDecoder = NULL;
                } else if (p_webm->codec_id == NESTEGG_CODEC_VORBIS) {
                        vorbis_block_clear(&p_webm->vorbisBlock);
                        vorbis_dsp_clear(&p_webm->vorbisDSP);
                        vorbis_comment_clear(&p_webm->vorbisComment);
                        vorbis_info_clear(&p_webm->vorbisInfo);
                }

                if (p_webm->ctx) {
                        nestegg_destroy(p_webm->ctx);
                        p_webm->ctx = NULL;
                }
        }
#else
        {
                /* webm is disabled. Should never hit this since initialization would have failed. */
                MA_ASSERT(MA_FALSE);
        }
#endif

        ma_data_source_uninit(&p_webm->ds);
}

MA_API ma_result ma_webm_read_pcm_frames(ma_webm *p_webm, void *p_frames_out, ma_uint64 frame_count, ma_uint64 *p_frames_read)
{
        if (p_frames_read)
                *p_frames_read = 0;
        if (frame_count == 0 || p_webm == NULL)
                return MA_INVALID_ARGS;

#if !defined(MA_NO_WEBM)
        ma_result result = MA_SUCCESS;
        ma_uint64 totalFramesRead = 0;
        ma_uint32 channels = p_webm->channels;

        float *f32Out = (float *)p_frames_out;

        float decodeBuf[MAX_OPUS_SAMPLES * MAX_OPUS_CHANNELS]; // Support up to 8 channels

        ma_uint64 seekTarget = (p_webm->seekTargetPCMFrame != (ma_uint64)-1) ? p_webm->seekTargetPCMFrame : 0;

        while (totalFramesRead < frame_count) {
                ma_uint64 framesNeeded = frame_count - totalFramesRead;

                // If there's a cached packet/frame in progress, decode that
                if (!p_webm->hasPacket) {
                        nestegg_packet *pkt = NULL;
                        // Next audio packet...

                        while (nestegg_read_packet(p_webm->ctx, &pkt) > 0) {
                                unsigned int track;
                                nestegg_packet_track(pkt, &track);
                                if (track == p_webm->audio_track) {
                                        p_webm->currentPacket = pkt;
                                        p_webm->currentPacketFrame = 0;
                                        p_webm->numFramesInPacket = 0;
                                        nestegg_packet_count(pkt, &p_webm->numFramesInPacket);
                                        p_webm->hasPacket = MA_TRUE;
                                        break;
                                }
                                nestegg_free_packet(pkt); // not audio, discard
                        }
                        if (!p_webm->hasPacket) {
                                result = MA_AT_END; // no more data
                                break;
                        }
                }

                // Decode remaining frames in this packet/frame
                nestegg_packet *pkt = p_webm->currentPacket;
                while (p_webm->currentPacketFrame < p_webm->numFramesInPacket && totalFramesRead < frame_count) {
                        unsigned char *data = NULL;
                        size_t dataSize = 0;
                        nestegg_packet_data(pkt, p_webm->currentPacketFrame, &data, &dataSize);

                        int nframes = 0;

                        if (p_webm->codec_id == NESTEGG_CODEC_OPUS) {
                                if (p_webm->bufferLeftoverFrameCount > 0) {
                                        ma_uint64 frames_to_copy = p_webm->bufferLeftoverFrameCount < framesNeeded ? p_webm->bufferLeftoverFrameCount : framesNeeded;

                                        memcpy(f32Out + totalFramesRead * channels,
                                               opusLeftoverBuffer + p_webm->bufferLeftoverFrameOffset * channels,
                                               frames_to_copy * channels * sizeof(float));

                                        p_webm->bufferLeftoverFrameOffset += frames_to_copy;
                                        totalFramesRead += frames_to_copy;
                                        framesNeeded -= frames_to_copy;
                                        p_webm->bufferLeftoverFrameCount -= frames_to_copy;

                                        if (p_webm->bufferLeftoverFrameCount == 0)
                                                p_webm->bufferLeftoverFrameOffset = 0;

                                        if (framesNeeded == 0)
                                                break;
                                }

                                nframes = opus_decode_float(p_webm->opusDecoder, data, (opus_int32)dataSize, decodeBuf, 5760, 0);

                                if (nframes < 0) {
                                        result = MA_ERROR;
                                        break;
                                }

                                ma_uint64 skipFrames = 0;
                                ma_uint64 usableFrames = 0;

                                // On first packets, discard enough to fulfill pre-skip value
                                if (p_webm->preSkipLeft > 0) {
                                        if ((ma_uint64)nframes <= p_webm->preSkipLeft) {
                                                // All output is to be skipped
                                                p_webm->preSkipLeft -= (ma_uint16)nframes;
                                                p_webm->cursorInPCMFrames += nframes;
                                                // Don't copy anything to output buffer
                                                goto NextFrame;
                                        } else {
                                                // Skip part, keep rest
                                                skipFrames = p_webm->preSkipLeft;
                                                p_webm->preSkipLeft = 0;
                                        }
                                } else if (seekTarget != (ma_uint64)-1 && p_webm->cursorInPCMFrames < seekTarget) {
                                        skipFrames = seekTarget - p_webm->cursorInPCMFrames;
                                        if (skipFrames > (ma_uint64)nframes)
                                                skipFrames = nframes;
                                }

                                usableFrames = (ma_uint64)nframes - skipFrames;
                                if (usableFrames > frame_count - totalFramesRead)
                                        usableFrames = frame_count - totalFramesRead;

                                // Only copy if there are any usable frames left
                                if (usableFrames > 0) {
                                        memcpy(
                                            f32Out + totalFramesRead * channels,
                                            decodeBuf + skipFrames * channels,
                                            usableFrames * channels * sizeof(float));
                                        totalFramesRead += usableFrames;
                                }

                                ma_uint64 framesUsed = skipFrames + usableFrames;
                                ma_uint64 frames_left = nframes - framesUsed;
                                if (frames_left > 0) {
                                        memcpy(opusLeftoverBuffer,
                                               decodeBuf + framesUsed * channels,
                                               frames_left * channels * sizeof(float));
                                        p_webm->bufferLeftoverFrameCount = frames_left;
                                        p_webm->bufferLeftoverFrameOffset = 0;
                                } else {
                                        p_webm->bufferLeftoverFrameCount = 0;
                                        p_webm->bufferLeftoverFrameOffset = 0;
                                }

                                // Always advance the PCM cursor by all decoded frames (skipped + copied)
                                p_webm->cursorInPCMFrames += (ma_uint64)nframes;

                                // If we've finished discarding, clear seek mode ("not discarding anymore")
                                if (seekTarget != (ma_uint64)-1 && p_webm->cursorInPCMFrames >= seekTarget) {
                                        p_webm->seekTargetPCMFrame = (ma_uint64)-1;
                                }

                        NextFrame:;
                        } else if (p_webm->codec_id == NESTEGG_CODEC_VORBIS) {
                                ogg_packet oggPkt = {0};
                                oggPkt.packet = data;
                                oggPkt.bytes = (long)dataSize;
                                oggPkt.b_o_s = (p_webm->currentPacketFrame == 0) ? 1 : 0;
                                oggPkt.e_o_s = 0;
                                oggPkt.granulepos = -1;

                                if (p_webm->bufferLeftoverFrameCount > 0) {
                                        ma_uint32 avail = p_webm->bufferLeftoverFrameCount - p_webm->bufferLeftoverFrameOffset;
                                        ma_uint32 toCopy = (frame_count - totalFramesRead) < avail ? (frame_count - totalFramesRead) : avail;
                                        memcpy(
                                            f32Out + totalFramesRead * channels,
                                            vorbisLeftoverBuffer + p_webm->bufferLeftoverFrameOffset * channels,
                                            toCopy * channels * sizeof(float));
                                        p_webm->bufferLeftoverFrameOffset += toCopy;
                                        totalFramesRead += toCopy;
                                        if (p_webm->bufferLeftoverFrameOffset == p_webm->bufferLeftoverFrameCount) {
                                                p_webm->bufferLeftoverFrameCount = 0;
                                                p_webm->bufferLeftoverFrameOffset = 0;
                                        }
                                        if (totalFramesRead >= frame_count)
                                                break; // Buffer full
                                }

                                int ret = vorbis_synthesis(&p_webm->vorbisBlock, &oggPkt);
                                if (ret == 0) {
                                        vorbis_synthesis_blockin(&p_webm->vorbisDSP, &p_webm->vorbisBlock);
                                        float **pcm;
                                        int framesAvail = vorbis_synthesis_pcmout(&p_webm->vorbisDSP, &pcm);
                                        if (framesAvail > 0) {
                                                ma_uint64 skipFrames = 0;
                                                if (seekTarget != (ma_uint64)-1 && p_webm->cursorInPCMFrames < seekTarget) {
                                                        skipFrames = seekTarget - p_webm->cursorInPCMFrames;
                                                        if (skipFrames > (ma_uint64)framesAvail)
                                                                skipFrames = framesAvail;
                                                }
                                                ma_uint64 usableFrames = (ma_uint64)framesAvail - skipFrames;
                                                if (usableFrames > frame_count - totalFramesRead)
                                                        usableFrames = frame_count - totalFramesRead;

                                                while (framesAvail > 0 && totalFramesRead < frame_count) {
                                                        ma_uint64 frames_to_copy = (frame_count - totalFramesRead) < (ma_uint64)framesAvail ? (frame_count - totalFramesRead) : (ma_uint64)framesAvail;

                                                        // Interleave frames_to_copy to output buffer directly
                                                        for (ma_uint64 f = 0; f < frames_to_copy; ++f)
                                                                for (ma_uint32 c = 0; c < channels; ++c)
                                                                        f32Out[(totalFramesRead + f) * channels + c] = pcm[c][f];

                                                        totalFramesRead += frames_to_copy;
                                                        framesAvail -= frames_to_copy;

                                                        // If left-over decoded frames after output buffer fills, write to leftover
                                                        if (framesAvail > 0) {
                                                                for (ma_uint32 f = 0; f < (ma_uint64)framesAvail; ++f)
                                                                        for (ma_uint32 c = 0; c < channels; ++c)
                                                                                vorbisLeftoverBuffer[f * channels + c] = pcm[c][frames_to_copy + f];
                                                                p_webm->bufferLeftoverFrameCount = (ma_uint64)framesAvail;
                                                                p_webm->bufferLeftoverFrameOffset = 0;
                                                                framesAvail = 0;
                                                                // Don't call vorbis_synthesis_read or increment cursor yet, do after finished with all available data!
                                                        }

                                                        // Consume these frames, even if we buffered them
                                                        vorbis_synthesis_read(&p_webm->vorbisDSP, frames_to_copy + p_webm->bufferLeftoverFrameCount); // or just all at once depending on your loop
                                                        p_webm->cursorInPCMFrames += (ma_uint64)(frames_to_copy + p_webm->bufferLeftoverFrameCount);
                                                        break; // Output full, let next call handle leftovers
                                                }

                                                // Always read/consume all frames we got (even those discarded)
                                                vorbis_synthesis_read(&p_webm->vorbisDSP, (int)framesAvail);
                                                p_webm->cursorInPCMFrames += (ma_uint64)framesAvail;

                                                // Done seeking?
                                                if (seekTarget != (ma_uint64)-1 && p_webm->cursorInPCMFrames >= seekTarget) {
                                                        p_webm->seekTargetPCMFrame = (ma_uint64)-1;
                                                }
                                        }
                                }
                        }

                        ++p_webm->currentPacketFrame;
                }

                if (p_webm->currentPacketFrame >= p_webm->numFramesInPacket) {
                        if (p_webm->currentPacket)
                                nestegg_free_packet(p_webm->currentPacket);
                        p_webm->currentPacket = NULL;
                        p_webm->hasPacket = MA_FALSE;
                }
        }

        if (totalFramesRead < frame_count) {
                memset(f32Out + totalFramesRead * channels, 0, (frame_count - totalFramesRead) * channels * sizeof(float));
        }

        if (p_frames_read)
                *p_frames_read = totalFramesRead;
        if (result == MA_SUCCESS && totalFramesRead == 0)
                result = MA_AT_END;
        return result;

#else
        {
                MA_ASSERT(MA_FALSE);
                (void)p_frames_out;
                (void)frame_count;
                (void)p_frames_read;
                return MA_NOT_IMPLEMENTED;
        }
#endif
}

MA_API ma_result ma_webm_seek_to_pcm_frame(ma_webm *p_webm, ma_uint64 frame_index)
{
        if (!p_webm)
                return MA_INVALID_ARGS;

        // For Opus: 80ms preroll = 3840 @ 48000Hz
        ma_uint64 preroll = 0;
        ma_uint64 prerollFrame = frame_index;
        ma_uint64 tstamp_ns = 0;

        if (p_webm->codec_id == NESTEGG_CODEC_OPUS) {
                preroll = (frame_index > 3840) ? 3840 : frame_index;
                prerollFrame = (frame_index > preroll) ? (frame_index - preroll) : 0;
                tstamp_ns = (prerollFrame * 1000000000ULL) / 48000;
        } else {
                prerollFrame = frame_index;
                tstamp_ns = (prerollFrame * 1000000000ULL) / p_webm->sample_rate;
        }

        if (nestegg_track_seek(p_webm->ctx, p_webm->audio_track, tstamp_ns) != 0)
                return MA_INVALID_OPERATION;

        // Reset packet and decoder state
        p_webm->hasPacket = MA_FALSE;
        if (p_webm->currentPacket) {
                nestegg_free_packet(p_webm->currentPacket);
                p_webm->currentPacket = NULL;
        }
        p_webm->currentPacketFrame = 0;
        p_webm->numFramesInPacket = 0;

        if (p_webm->codec_id == NESTEGG_CODEC_OPUS)
                opus_decoder_ctl(p_webm->opusDecoder, OPUS_RESET_STATE);
        else if (p_webm->codec_id == NESTEGG_CODEC_VORBIS) {
                vorbis_dsp_clear(&p_webm->vorbisDSP);
                vorbis_block_clear(&p_webm->vorbisBlock);
                vorbis_synthesis_init(&p_webm->vorbisDSP, &p_webm->vorbisInfo);
                vorbis_block_init(&p_webm->vorbisDSP, &p_webm->vorbisBlock);
        }

        p_webm->bufferLeftoverFrameCount = 0;
        p_webm->bufferLeftoverFrameOffset = 0;

        p_webm->cursorInPCMFrames = prerollFrame;
        p_webm->seekTargetPCMFrame = frame_index;

        if (p_webm->seekTargetPCMFrame == 0)
                p_webm->preSkipLeft = p_webm->opusPreSkip;
        else
                p_webm->preSkipLeft = 0;

        return MA_SUCCESS;
}
MA_API ma_result ma_webm_get_data_format(
    ma_webm *p_webm,
    ma_format *p_format,
    ma_uint32 *p_channels,
    ma_uint32 *p_sample_rate,
    ma_channel *p_channel_map,
    size_t channel_map_cap)
{
        /* Defaults for safety. */
        if (p_format != NULL)
                *p_format = ma_format_unknown;
        if (p_channels != NULL)
                *p_channels = 0;
        if (p_sample_rate != NULL)
                *p_sample_rate = 0;
        if (p_channel_map != NULL)
                MA_ZERO_MEMORY(p_channel_map, sizeof(*p_channel_map) * channel_map_cap);
        if (p_webm == NULL)
                return MA_INVALID_OPERATION;

        if (p_format != NULL)
                *p_format = p_webm->format;

#if !defined(MA_NO_WEBM)
        {
                if (p_channels != NULL) {
                        *p_channels = p_webm->channels;
                }

                if (p_sample_rate != NULL) {
                        *p_sample_rate = p_webm->sample_rate;
                }

                if (p_channel_map != NULL) {
                        if (p_channel_map != NULL) {
                                ma_channel_map_init_standard(
                                    ma_standard_channel_map_vorbis,
                                    p_channel_map,
                                    channel_map_cap,
                                    p_webm->channels);
                        }
                }

                return MA_SUCCESS;
        }
#else
        {
                MA_ASSERT(MA_FALSE);
                return MA_NOT_IMPLEMENTED;
        }
#endif
}

MA_API ma_result ma_webm_get_cursor_in_pcm_frames(ma_webm *p_webm, ma_uint64 *p_cursor)
{
        if (p_cursor == NULL || p_webm == NULL) {
                return MA_INVALID_ARGS;
        }

#if !defined(MA_NO_WEBM)
        {
                *p_cursor = p_webm->cursorInPCMFrames;
                return MA_SUCCESS;
        }
#else
        {
                MA_ASSERT(MA_FALSE);
                return MA_NOT_IMPLEMENTED;
        }
#endif
}

ma_uint64 calculate_length_in_pcm_frames(ma_webm *p_webm)
{
        uint64_t duration_ns = 0;
        if (nestegg_duration(p_webm->ctx, &duration_ns) == 0 && duration_ns > 0) {
                // For Opus, duration_ns is always in 48kHz timebase per WebM spec
                if (p_webm->codec_id == NESTEGG_CODEC_OPUS) {
                        // Convert nanoseconds to 48kHz PCM frames
                        uint64_t total_frames_48k = (duration_ns * 48000ull) / 1000000000ull;

                        // Subtract pre-skip and trimming (if known)
                        uint64_t pre_skip = p_webm->opusPreSkip;

                        if (total_frames_48k > pre_skip)
                                total_frames_48k -= pre_skip;

                        return total_frames_48k;
                } else {
                        // For Vorbis and others, just use sample_rate
                        return (ma_uint64)((duration_ns * (uint64_t)p_webm->sample_rate) / 1000000000ull);
                }
        }
        return 0;
}

MA_API ma_result ma_webm_get_length_in_pcm_frames(ma_webm *p_webm, ma_uint64 *p_length)
{
        if (p_length == NULL || p_webm == NULL) {
                return MA_INVALID_ARGS;
        }

#if !defined(MA_NO_WEBM)
        {
                if (p_webm->lengthInPCMFrames == 0) {
                        p_webm->lengthInPCMFrames = calculate_length_in_pcm_frames(p_webm);
                }
                *p_length = p_webm->lengthInPCMFrames;
                return MA_SUCCESS;
        }
#else
        {
                MA_ASSERT(MA_FALSE);
                return MA_NOT_IMPLEMENTED;
        }
#endif
}

#endif
