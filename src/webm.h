#ifndef WEBM_H
#define WEBM_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "miniaudio.h"

#if !defined(MA_NO_WEBM)
#include <opusfile.h>
#include <vorbis/codec.h>
#include <nestegg/nestegg.h>

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
                ma_uint64 audioTrack;
                ma_uint64 cursorInPCMFrames;
                ma_uint64 seekTargetPCMFrame;
                ma_uint32 sampleRate;
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

#endif
        } ma_webm;

        MA_API ma_result ma_webm_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void *pReadSeekTellUserData, const ma_decoding_backend_config *pConfig, const ma_allocation_callbacks *pAllocationCallbacks, ma_webm *pWebm);
        MA_API ma_result ma_webm_init_file(const char *pFilePath, const ma_decoding_backend_config *pConfig, const ma_allocation_callbacks *pAllocationCallbacks, ma_webm *pWebm);
        MA_API void ma_webm_uninit(ma_webm *pOpus, const ma_allocation_callbacks *pAllocationCallbacks);
        MA_API ma_result ma_webm_read_pcm_frames(ma_webm *pWebm, void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead);
        MA_API ma_result ma_webm_seek_to_pcm_frame(ma_webm *pWebm, ma_uint64 frameIndex);
        MA_API ma_result ma_webm_get_data_format(ma_webm *pOpus, ma_format *pFormat, ma_uint32 *pChannels, ma_uint32 *pSampleRate, ma_channel *pChannelMap, size_t channelMapCap);
        MA_API ma_result ma_webm_get_cursor_in_pcm_frames(ma_webm *pWebm, ma_uint64 *pCursor);
        MA_API ma_result ma_webm_get_length_in_pcm_frames(ma_webm *pWebm, ma_uint64 *pLength);

#ifdef __cplusplus
}
#endif
#endif

#if defined(MINIAUDIO_IMPLEMENTATION) || defined(MA_IMPLEMENTATION)

#define MAX_OPUS_CHANNELS 8
#define MAX_OPUS_SAMPLES 5760 // Maximum expected frame size

static float opusLeftoverBuffer[MAX_OPUS_SAMPLES * MAX_OPUS_CHANNELS];

static ma_uint64 bufferLeftoverFrameCount = 0;
static ma_uint64 bufferLeftoverFrameOffset = 0;

#define MAX_VORBIS_PACKET_FRAMES 4096
#define MAX_VORBIS_CHANNELS 8

float vorbisLeftoverBuffer[MAX_VORBIS_PACKET_FRAMES * MAX_VORBIS_CHANNELS];

static ma_result ma_webm_ds_read(ma_data_source *pDataSource, void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead)
{
        return ma_webm_read_pcm_frames((ma_webm *)pDataSource, pFramesOut, frameCount, pFramesRead);
}

static ma_result ma_webm_ds_seek(ma_data_source *pDataSource, ma_uint64 frameIndex)
{
        return ma_webm_seek_to_pcm_frame((ma_webm *)pDataSource, frameIndex);
}

static ma_result ma_webm_ds_get_data_format(ma_data_source *pDataSource, ma_format *pFormat, ma_uint32 *pChannels, ma_uint32 *pSampleRate, ma_channel *pChannelMap, size_t channelMapCap)
{
        return ma_webm_get_data_format((ma_webm *)pDataSource, pFormat, pChannels, pSampleRate, pChannelMap, channelMapCap);
}

static ma_result ma_webm_ds_get_cursor(ma_data_source *pDataSource, ma_uint64 *pCursor)
{
        return ma_webm_get_cursor_in_pcm_frames((ma_webm *)pDataSource, pCursor);
}

static ma_result ma_webm_ds_get_length(ma_data_source *pDataSource, ma_uint64 *pLength)
{
        return ma_webm_get_length_in_pcm_frames((ma_webm *)pDataSource, pLength);
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

#if !defined(MA_NO_WEBM)
static int ma_webm_of_callback__read(void *pUserData, unsigned char *pBufferOut, int bytesToRead)
{
        ma_webm *pWebm = (ma_webm *)pUserData;
        ma_result result;
        size_t bytesRead;

        result = pWebm->onRead(pWebm->pReadSeekTellUserData, (void *)pBufferOut, bytesToRead, &bytesRead);

        if (result != MA_SUCCESS)
        {
                return -1;
        }

        return (int)bytesRead;
}

static int ma_webm_of_callback__seek(void *pUserData, ogg_int64_t offset, int whence)
{
        ma_webm *pWebm = (ma_webm *)pUserData;
        ma_result result;
        ma_seek_origin origin;

        if (whence == SEEK_SET)
        {
                origin = ma_seek_origin_start;
        }
        else if (whence == SEEK_END)
        {
                origin = ma_seek_origin_end;
        }
        else
        {
                origin = ma_seek_origin_current;
        }

        result = pWebm->onSeek(pWebm->pReadSeekTellUserData, offset, origin);
        if (result != MA_SUCCESS)
        {
                return -1;
        }

        return 0;
}

static opus_int64 ma_webm_of_callback__tell(void *pUserData)
{
        ma_webm *pWebm = (ma_webm *)pUserData;
        ma_result result;
        ma_int64 cursor;

        if (pWebm->onTell == NULL)
        {
                return -1;
        }

        result = pWebm->onTell(pWebm->pReadSeekTellUserData, &cursor);
        if (result != MA_SUCCESS)
        {
                return -1;
        }

        return cursor;
}
#endif

static ma_result ma_webm_init_internal(const ma_decoding_backend_config *pConfig, ma_webm *pWebm)
{
        ma_result result;
        ma_data_source_config dataSourceConfig;

        if (pWebm == NULL)
        {
                return MA_INVALID_ARGS;
        }

        MA_ZERO_OBJECT(pWebm);
        pWebm->format = ma_format_f32; // f32 by default.
        pWebm->seekTargetPCMFrame = (ma_uint64)-1;

        // Clear leftover buffer
        bufferLeftoverFrameCount = 0;
        bufferLeftoverFrameOffset = 0;

        if (pConfig != NULL && (pConfig->preferredFormat == ma_format_f32 || pConfig->preferredFormat == ma_format_s16))
        {
                pWebm->format = pConfig->preferredFormat;
        }
        else
        {
                /* Getting here means something other than f32 and s16 was specified. Just leave this unset to use the default format. */
        }

        dataSourceConfig = ma_data_source_config_init();
        dataSourceConfig.vtable = &g_ma_webm_ds_vtable;

        result = ma_data_source_init(&dataSourceConfig, &pWebm->ds);
        if (result != MA_SUCCESS)
        {
                return result; /* Failed to initialize the base data source. */
        }

        return MA_SUCCESS;
}

static int nestegg_io_read(void *buffer, size_t length, void *userdata)
{
        ma_webm *webm = (ma_webm *)userdata;
        size_t bytesRead = 0;
        if (webm->onRead(webm->pReadSeekTellUserData, buffer, length, &bytesRead) == MA_SUCCESS)
                return (bytesRead == length) ? 1 : 0;
        return -1;
}

static int nestegg_io_seek(int64_t offset, int whence, void *userdata)
{
        ma_webm *webm = (ma_webm *)userdata;
        ma_seek_origin origin;
        switch (whence)
        {
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
        if (nestegg_duration(ctx, &duration_ns) == 0)
        {
                duration = (double)duration_ns / 1e9;
        }
        return duration;
}

static int ma_webm_init_vorbis_decoder(
    nestegg *ctx,
    unsigned int audioTrack,
    ma_webm *pWebm)
{
        unsigned char *id = NULL, *comment = NULL, *setup = NULL;
        size_t id_size = 0, comment_size = 0, setup_size = 0;
        ogg_packet header_packet;

        // Fetch header packets as delivered by WebM/Matroska.
        if (nestegg_track_codec_data(ctx, audioTrack, 0, &id, &id_size) != 0 ||
            nestegg_track_codec_data(ctx, audioTrack, 1, &comment, &comment_size) != 0 ||
            nestegg_track_codec_data(ctx, audioTrack, 2, &setup, &setup_size) != 0)
        {
                return -1; // invalid file or track
        }

        // Setup libvorbis structures.
        vorbis_info_init(&pWebm->vorbisInfo);
        vorbis_comment_init(&pWebm->vorbisComment);

        memset(&header_packet, 0, sizeof(header_packet));
        // Header 1: ID
        header_packet.packet = id;
        header_packet.bytes = id_size;
        header_packet.b_o_s = 1;
        header_packet.e_o_s = 0;
        if (vorbis_synthesis_headerin(&pWebm->vorbisInfo, &pWebm->vorbisComment, &header_packet) != 0)
                goto fail;

        // Header 2: COMMENT
        header_packet.packet = comment;
        header_packet.bytes = comment_size;
        header_packet.b_o_s = 0;
        // header_packet.e_o_s remains 0
        if (vorbis_synthesis_headerin(&pWebm->vorbisInfo, &pWebm->vorbisComment, &header_packet) != 0)
                goto fail;

        // Header 3: SETUP
        header_packet.packet = setup;
        header_packet.bytes = setup_size;
        // header_packet.b_o_s remains 0
        if (vorbis_synthesis_headerin(&pWebm->vorbisInfo, &pWebm->vorbisComment, &header_packet) != 0)
                goto fail;

        // Setup decoder
        if (vorbis_synthesis_init(&pWebm->vorbisDSP, &pWebm->vorbisInfo) != 0)
                goto fail;
        if (vorbis_block_init(&pWebm->vorbisDSP, &pWebm->vorbisBlock) != 0)
                goto fail;

        pWebm->channels = pWebm->vorbisInfo.channels;
        pWebm->sampleRate = pWebm->vorbisInfo.rate;
        pWebm->format = ma_format_f32;

        return 0; // success

fail:
        vorbis_block_clear(&pWebm->vorbisBlock);
        vorbis_dsp_clear(&pWebm->vorbisDSP);
        vorbis_comment_clear(&pWebm->vorbisComment);
        vorbis_info_clear(&pWebm->vorbisInfo);
        return -2; // error
}

MA_API ma_result ma_webm_init(
    ma_read_proc onRead,
    ma_seek_proc onSeek,
    ma_tell_proc onTell,
    void *pReadSeekTellUserData,
    const ma_decoding_backend_config *pConfig,
    const ma_allocation_callbacks *pAllocationCallbacks,
    ma_webm *pWebm)
{
        ma_result result;

        (void)pAllocationCallbacks;

        result = ma_webm_init_internal(pConfig, pWebm);
        if (result != MA_SUCCESS)
        {
                return result;
        }

        if (onRead == NULL || onSeek == NULL)
        {
                return MA_INVALID_ARGS; /* onRead and onSeek are mandatory. */
        }

        pWebm->onRead = onRead;
        pWebm->onSeek = onSeek;
        pWebm->onTell = onTell;
        pWebm->pReadSeekTellUserData = pReadSeekTellUserData;

#if !defined(MA_NO_WEBM)
        nestegg_io io = {0};

        // Adapter functions for nestegg
        io.read = nestegg_io_read;
        io.seek = nestegg_io_seek;
        io.tell = nestegg_io_tell;
        io.userdata = pWebm;

        nestegg *ctx = NULL;
        if (nestegg_init(&ctx, io, NULL, -1) < 0)
        {
                return MA_INVALID_FILE;
        }

        // Find Audio Track
        unsigned int num_tracks = 0;
        if (nestegg_track_count(ctx, &num_tracks) != 0)
        {
                nestegg_destroy(ctx);
                return MA_INVALID_FILE;
        }

        pWebm->audioTrack = (unsigned int)-1;
        pWebm->codec_id = -1;
        for (unsigned int i = 0; i < num_tracks; ++i)
        {
                unsigned int type = 0;
                type = nestegg_track_type(ctx, i);
                if (type == NESTEGG_TRACK_AUDIO)
                {
                        pWebm->audioTrack = i;
                        pWebm->codec_id = nestegg_track_codec_id(ctx, i);
                        if (pWebm->codec_id == 0)
                        {
                                break; // first audio
                        }
                }
        }

        if (pWebm->audioTrack == (unsigned int)-1)
        {
                nestegg_destroy(ctx);
                return MA_INVALID_FILE;
        }

        // Prepare decoder
        if (pWebm->codec_id == NESTEGG_CODEC_OPUS)
        {
                unsigned char *header = NULL;
                size_t header_size = 0;
                if (nestegg_track_codec_data(ctx, pWebm->audioTrack, 0, &header, &header_size) != 0 ||
                    header_size < 19 || memcmp(header, "OpusHead", 8) != 0)
                {
                        nestegg_destroy(ctx);
                        return MA_INVALID_FILE;
                }
                pWebm->sampleRate = 48000;
                pWebm->channels = header[9];
                ma_uint16 preSkip = header[10] | (header[11] << 8); // Little-endian
                pWebm->opusPreSkip = preSkip;
                pWebm->preSkipLeft = preSkip;
                int opusErr = 0;
                pWebm->opusDecoder = opus_decoder_create(pWebm->sampleRate, pWebm->channels, &opusErr);
                if (!pWebm->opusDecoder)
                {
                        nestegg_destroy(ctx);
                        return MA_INVALID_FILE;
                }
                pWebm->format = ma_format_f32;
        }
        else if (pWebm->codec_id == NESTEGG_CODEC_VORBIS)
        {
                if (ma_webm_init_vorbis_decoder(ctx, pWebm->audioTrack, pWebm) != 0)
                {
                        nestegg_destroy(ctx);
                        return MA_INVALID_FILE;
                }
        }
        else
        {
                nestegg_destroy(ctx);
                return MA_NOT_IMPLEMENTED;
        }

        pWebm->ctx = ctx;

        pWebm->duration = calcWebmDuration(ctx);
        pWebm->seekTargetPCMFrame = (ma_uint64)(-1);

        return MA_SUCCESS;

#else
        (void)pReadSeekTellUserData;
        (void)pConfig;
        (void)pWebm;
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
        switch (w)
        {
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

MA_API ma_result ma_webm_init_file(const char *pFilePath, const ma_decoding_backend_config *pConfig, const ma_allocation_callbacks *pAllocationCallbacks, ma_webm *pWebm)
{
        ma_result result;

        (void)pAllocationCallbacks;

        result = ma_webm_init_internal(pConfig, pWebm);
        if (result != MA_SUCCESS)
        {
                return result;
        }

#if !defined(MA_NO_WEBM)
        FILE *fp = fopen(pFilePath, "rb");
        if (!fp)
                return MA_INVALID_FILE;

        nestegg_io io = {nread, nseek, ntell, fp};
        nestegg *ctx = NULL;

        if (nestegg_init(&ctx, io, NULL, -1) < 0)
        {
                fclose(fp);
                return MA_INVALID_FILE;
        }

        unsigned int num_tracks = 0;
        nestegg_track_count(ctx, &num_tracks);
        pWebm->audioTrack = (unsigned int)(-1);
        pWebm->codec_id = -1;
        for (unsigned int i = 0; i < num_tracks; ++i)
        {
                unsigned int type = 0;
                type = nestegg_track_type(ctx, i);
                if (type == NESTEGG_TRACK_AUDIO)
                {
                        pWebm->audioTrack = i;
                        pWebm->codec_id = nestegg_track_codec_id(ctx, i);
                        break;
                }
        }
        if (pWebm->audioTrack == (unsigned int)(-1))
        {
                nestegg_destroy(ctx);
                fclose(fp);
                return MA_ERROR;
        }

        // Fetch and handle header
        if (pWebm->codec_id == NESTEGG_CODEC_OPUS)
        {
                unsigned char *header = NULL;
                size_t header_size = 0;
                nestegg_track_codec_data(ctx, pWebm->audioTrack, 0, &header, &header_size);
                if (header_size < 19 || memcmp(header, "OpusHead", 8) != 0)
                {
                        nestegg_destroy(ctx);
                        fclose(fp);
                        return MA_ERROR;
                }
                pWebm->channels = header[9];
                ma_uint16 preSkip = header[10] | (header[11] << 8); // Little-endian

                pWebm->opusPreSkip = preSkip;
                pWebm->preSkipLeft = preSkip;
                int opusErr = 0;

                pWebm->opusDecoder = opus_decoder_create(48000, pWebm->channels, &opusErr);
                if (!pWebm->opusDecoder)
                {
                        nestegg_destroy(ctx);
                        fclose(fp);
                        return MA_ERROR;
                }

                pWebm->format = ma_format_f32;
                pWebm->sampleRate = 48000;
        }
        else if (pWebm->codec_id == NESTEGG_CODEC_VORBIS)
        {
                if (ma_webm_init_vorbis_decoder(ctx, pWebm->audioTrack, pWebm) != 0)
                {
                        nestegg_destroy(ctx);
                        return MA_INVALID_FILE;
                }
        }
        else
        {
                nestegg_destroy(ctx);
                fclose(fp);
                return MA_NOT_IMPLEMENTED;
        }

        pWebm->ctx = ctx;

        pWebm->duration = calcWebmDuration(ctx);
        pWebm->seekTargetPCMFrame = (ma_uint64)(-1);

        return MA_SUCCESS;

#else
        /* webm is disabled. */
        (void)pFilePath;
        return MA_NOT_IMPLEMENTED;
#endif
}

MA_API void ma_webm_uninit(ma_webm *pWebm, const ma_allocation_callbacks *pAllocationCallbacks)
{
        if (pWebm == NULL)
        {
                return;
        }

        (void)pAllocationCallbacks;

#if !defined(MA_NO_WEBM)
        {
                if (pWebm->codec_id == NESTEGG_CODEC_OPUS)
                {
                        opus_decoder_destroy(pWebm->opusDecoder);
                        pWebm->opusDecoder = NULL;
                }
                else if (pWebm->codec_id == NESTEGG_CODEC_VORBIS)
                {
                        vorbis_block_clear(&pWebm->vorbisBlock);
                        vorbis_dsp_clear(&pWebm->vorbisDSP);
                        vorbis_comment_clear(&pWebm->vorbisComment);
                        vorbis_info_clear(&pWebm->vorbisInfo);
                }

                if (pWebm->ctx)
                {
                        nestegg_destroy(pWebm->ctx);
                        pWebm->ctx = NULL;
                }
        }
#else
        {
                /* webm is disabled. Should never hit this since initialization would have failed. */
                MA_ASSERT(MA_FALSE);
        }
#endif

        ma_data_source_uninit(&pWebm->ds);
}

MA_API ma_result ma_webm_read_pcm_frames(ma_webm *pWebm, void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead)
{
        if (pFramesRead)
                *pFramesRead = 0;
        if (frameCount == 0 || pWebm == NULL)
                return MA_INVALID_ARGS;

#if !defined(MA_NO_WEBM)
        ma_result result = MA_SUCCESS;
        ma_uint64 totalFramesRead = 0;
        ma_uint32 channels = pWebm->channels;

        float *f32Out = (float *)pFramesOut;

        float decodeBuf[MAX_OPUS_SAMPLES * MAX_OPUS_CHANNELS]; // Support up to 8 channels

        ma_uint64 seekTarget = (pWebm->seekTargetPCMFrame != (ma_uint64)-1) ? pWebm->seekTargetPCMFrame : 0;

        while (totalFramesRead < frameCount)
        {
                ma_uint64 framesNeeded = frameCount - totalFramesRead;

                // If there's a cached packet/frame in progress, decode that
                if (!pWebm->hasPacket)
                {
                        nestegg_packet *pkt = NULL;
                        // Next audio packet...
                        while (nestegg_read_packet(pWebm->ctx, &pkt) > 0)
                        {
                                unsigned int track;
                                nestegg_packet_track(pkt, &track);
                                if (track == pWebm->audioTrack)
                                {
                                        pWebm->currentPacket = pkt;
                                        pWebm->currentPacketFrame = 0;
                                        pWebm->numFramesInPacket = 0;
                                        nestegg_packet_count(pkt, &pWebm->numFramesInPacket);
                                        pWebm->hasPacket = MA_TRUE;
                                        break;
                                }
                                nestegg_free_packet(pkt); // not audio, discard
                        }
                        if (!pWebm->hasPacket)
                        {
                                result = MA_AT_END; // no more data
                                break;
                        }
                }

                // Decode remaining frames in this packet/frame
                nestegg_packet *pkt = pWebm->currentPacket;
                while (pWebm->currentPacketFrame < pWebm->numFramesInPacket && totalFramesRead < frameCount)
                {
                        unsigned char *data = NULL;
                        size_t dataSize = 0;
                        nestegg_packet_data(pkt, pWebm->currentPacketFrame, &data, &dataSize);

                        int nframes = 0;

                        if (pWebm->codec_id == NESTEGG_CODEC_OPUS)
                        {
                                if (bufferLeftoverFrameCount > 0)
                                {
                                        ma_uint64 framesToCopy = bufferLeftoverFrameCount < framesNeeded ? bufferLeftoverFrameCount : framesNeeded;

                                        memcpy(f32Out + totalFramesRead * channels,
                                               opusLeftoverBuffer + bufferLeftoverFrameOffset * channels,
                                               framesToCopy * channels * sizeof(float));

                                        bufferLeftoverFrameOffset += framesToCopy;
                                        totalFramesRead += framesToCopy;
                                        framesNeeded -= framesToCopy;
                                        bufferLeftoverFrameCount -= framesToCopy;

                                        if (bufferLeftoverFrameCount == 0)
                                                bufferLeftoverFrameOffset = 0;

                                        if (framesNeeded == 0)
                                                break;
                                }

                                nframes = opus_decode_float(pWebm->opusDecoder, data, (opus_int32)dataSize, decodeBuf, 5760, 0);

                                if (nframes < 0)
                                {
                                        result = MA_ERROR;
                                        break;
                                }

                                ma_uint64 skipFrames = 0;
                                ma_uint64 usableFrames = 0;

                                // On first packets, discard enough to fulfill pre-skip value
                                if (pWebm->preSkipLeft > 0)
                                {
                                        if ((ma_uint64)nframes <= pWebm->preSkipLeft)
                                        {
                                                // All output is to be skipped
                                                pWebm->preSkipLeft -= (ma_uint16)nframes;
                                                pWebm->cursorInPCMFrames += nframes;
                                                // Don't copy anything to output buffer
                                                goto NextFrame;
                                        }
                                        else
                                        {
                                                // Skip part, keep rest
                                                skipFrames = pWebm->preSkipLeft;
                                                pWebm->preSkipLeft = 0;
                                        }
                                }
                                else if (seekTarget != (ma_uint64)-1 && pWebm->cursorInPCMFrames < seekTarget)
                                {
                                        skipFrames = seekTarget - pWebm->cursorInPCMFrames;
                                        if (skipFrames > (ma_uint64)nframes)
                                                skipFrames = nframes;
                                }

                                usableFrames = (ma_uint64)nframes - skipFrames;
                                if (usableFrames > frameCount - totalFramesRead)
                                        usableFrames = frameCount - totalFramesRead;

                                // Only copy if there are any usable frames left
                                if (usableFrames > 0)
                                {
                                        memcpy(
                                            f32Out + totalFramesRead * channels,
                                            decodeBuf + skipFrames * channels,
                                            usableFrames * channels * sizeof(float));
                                        totalFramesRead += usableFrames;
                                }

                                ma_uint64 framesUsed = skipFrames + usableFrames;
                                ma_uint64 framesLeft = nframes - framesUsed;
                                if (framesLeft > 0)
                                {
                                        memcpy(opusLeftoverBuffer,
                                               decodeBuf + framesUsed * channels,
                                               framesLeft * channels * sizeof(float));
                                        bufferLeftoverFrameCount = framesLeft;
                                        bufferLeftoverFrameOffset = 0;
                                }
                                else
                                {
                                        bufferLeftoverFrameCount = 0;
                                        bufferLeftoverFrameOffset = 0;
                                }

                                // Always advance the PCM cursor by all decoded frames (skipped + copied)
                                pWebm->cursorInPCMFrames += (ma_uint64)nframes;

                                // If we've finished discarding, clear seek mode ("not discarding anymore")
                                if (seekTarget != (ma_uint64)-1 && pWebm->cursorInPCMFrames >= seekTarget)
                                {
                                        pWebm->seekTargetPCMFrame = (ma_uint64)-1;
                                }

                        NextFrame:;
                        }
                        else if (pWebm->codec_id == NESTEGG_CODEC_VORBIS)
                        {
                                ogg_packet oggPkt = {0};
                                oggPkt.packet = data;
                                oggPkt.bytes = (long)dataSize;
                                oggPkt.b_o_s = (pWebm->currentPacketFrame == 0) ? 1 : 0;
                                oggPkt.e_o_s = 0;
                                oggPkt.granulepos = -1;

                                if (bufferLeftoverFrameCount > 0)
                                {
                                        ma_uint32 avail = bufferLeftoverFrameCount - bufferLeftoverFrameOffset;
                                        ma_uint32 toCopy = (frameCount - totalFramesRead) < avail ? (frameCount - totalFramesRead) : avail;
                                        memcpy(
                                            f32Out + totalFramesRead * channels,
                                            vorbisLeftoverBuffer + bufferLeftoverFrameOffset * channels,
                                            toCopy * channels * sizeof(float));
                                        bufferLeftoverFrameOffset += toCopy;
                                        totalFramesRead += toCopy;
                                        if (bufferLeftoverFrameOffset == bufferLeftoverFrameCount)
                                        {
                                                bufferLeftoverFrameCount = 0;
                                                bufferLeftoverFrameOffset = 0;
                                        }
                                        if (totalFramesRead >= frameCount)
                                                break; // Buffer full
                                }

                                int ret = vorbis_synthesis(&pWebm->vorbisBlock, &oggPkt);
                                if (ret == 0)
                                {
                                        vorbis_synthesis_blockin(&pWebm->vorbisDSP, &pWebm->vorbisBlock);
                                        float **pcm;
                                        long framesAvail = vorbis_synthesis_pcmout(&pWebm->vorbisDSP, &pcm);
                                        if (framesAvail > 0)
                                        {
                                                ma_uint64 skipFrames = 0;
                                                if (seekTarget != (ma_uint64)-1 && pWebm->cursorInPCMFrames < seekTarget)
                                                {
                                                        skipFrames = seekTarget - pWebm->cursorInPCMFrames;
                                                        if (skipFrames > (ma_uint64)framesAvail)
                                                                skipFrames = framesAvail;
                                                }
                                                ma_uint64 usableFrames = (ma_uint64)framesAvail - skipFrames;
                                                if (usableFrames > frameCount - totalFramesRead)
                                                        usableFrames = frameCount - totalFramesRead;

                                                while (framesAvail > 0 && totalFramesRead < frameCount)
                                                {
                                                        ma_uint32 framesToCopy = (frameCount - totalFramesRead) < framesAvail ? (frameCount - totalFramesRead) : framesAvail;

                                                        // Interleave framesToCopy to output buffer directly
                                                        for (ma_uint32 f = 0; f < framesToCopy; ++f)
                                                                for (ma_uint32 c = 0; c < channels; ++c)
                                                                        f32Out[(totalFramesRead + f) * channels + c] = pcm[c][f];

                                                        totalFramesRead += framesToCopy;
                                                        framesAvail -= framesToCopy;

                                                        // If left-over decoded frames after output buffer fills, write to leftover
                                                        if (framesAvail > 0)
                                                        {
                                                                for (ma_uint32 f = 0; f < framesAvail; ++f)
                                                                        for (ma_uint32 c = 0; c < channels; ++c)
                                                                                vorbisLeftoverBuffer[f * channels + c] = pcm[c][framesToCopy + f];
                                                                bufferLeftoverFrameCount = framesAvail;
                                                                bufferLeftoverFrameOffset = 0;
                                                                framesAvail = 0;
                                                                // Don't call vorbis_synthesis_read or increment cursor yet, do after finished with all available data!
                                                        }

                                                        // Consume these frames, even if we buffered them
                                                        vorbis_synthesis_read(&pWebm->vorbisDSP, framesToCopy + bufferLeftoverFrameCount); // or just all at once depending on your loop
                                                        pWebm->cursorInPCMFrames += (ma_uint64)(framesToCopy + bufferLeftoverFrameCount);
                                                        break; // Output full, let next call handle leftovers
                                                }

                                                // Always read/consume all frames we got (even those discarded)
                                                vorbis_synthesis_read(&pWebm->vorbisDSP, (int)framesAvail);
                                                pWebm->cursorInPCMFrames += (ma_uint64)framesAvail;

                                                // Done seeking?
                                                if (seekTarget != (ma_uint64)-1 && pWebm->cursorInPCMFrames >= seekTarget)
                                                {
                                                        pWebm->seekTargetPCMFrame = (ma_uint64)-1;
                                                }
                                        }
                                }
                        }

                        ++pWebm->currentPacketFrame;
                }

                if (pWebm->currentPacketFrame >= pWebm->numFramesInPacket)
                {
                        if (pWebm->currentPacket)
                                nestegg_free_packet(pWebm->currentPacket);
                        pWebm->currentPacket = NULL;
                        pWebm->hasPacket = MA_FALSE;
                }
        }

        if (totalFramesRead < frameCount)
        {
                memset(f32Out + totalFramesRead * channels, 0, (frameCount - totalFramesRead) * channels * sizeof(float));
        }

        if (pFramesRead)
                *pFramesRead = totalFramesRead;
        if (result == MA_SUCCESS && totalFramesRead == 0)
                result = MA_AT_END;
        return result;

#else
        {
                MA_ASSERT(MA_FALSE);
                (void)pFramesOut;
                (void)frameCount;
                (void)pFramesRead;
                return MA_NOT_IMPLEMENTED;
        }
#endif
}

MA_API ma_result ma_webm_seek_to_pcm_frame(ma_webm *pWebm, ma_uint64 frameIndex)
{
        if (!pWebm)
                return MA_INVALID_ARGS;

        // For Opus: preroll ~80ms, as spec
        ma_uint64 preroll = 0;
        if (pWebm->codec_id == NESTEGG_CODEC_OPUS)
                preroll = (frameIndex > 3840) ? 3840 : frameIndex; // 80ms @ 48000Hz

        ma_uint64 prerollFrame = frameIndex - preroll;

        ma_uint64 tstamp_ns = (prerollFrame * 1000000000ULL) / pWebm->sampleRate;
        if (nestegg_offset_seek(pWebm->ctx, tstamp_ns) != 0)
                return MA_INVALID_OPERATION;

        // Reset packet and decoder state
        pWebm->hasPacket = MA_FALSE;
        if (pWebm->currentPacket)
        {
                nestegg_free_packet(pWebm->currentPacket);
                pWebm->currentPacket = NULL;
        }
        pWebm->currentPacketFrame = 0;
        pWebm->numFramesInPacket = 0;

        if (pWebm->codec_id == NESTEGG_CODEC_OPUS)
        {
                opus_decoder_ctl(pWebm->opusDecoder, OPUS_RESET_STATE);
        }
        else if (pWebm->codec_id == NESTEGG_CODEC_VORBIS)
        {
                vorbis_dsp_clear(&pWebm->vorbisDSP);
                vorbis_block_clear(&pWebm->vorbisBlock);
                vorbis_synthesis_init(&pWebm->vorbisDSP, &pWebm->vorbisInfo);
                vorbis_block_init(&pWebm->vorbisDSP, &pWebm->vorbisBlock);
        }

        // Clear leftover buffer
        bufferLeftoverFrameCount = 0;
        bufferLeftoverFrameOffset = 0;

        // Set cursor and target
        pWebm->cursorInPCMFrames = 0;
        pWebm->seekTargetPCMFrame = frameIndex;

        return MA_SUCCESS;
}

MA_API ma_result ma_webm_get_data_format(
    ma_webm *pWebm,
    ma_format *pFormat,
    ma_uint32 *pChannels,
    ma_uint32 *pSampleRate,
    ma_channel *pChannelMap,
    size_t channelMapCap)
{
        /* Defaults for safety. */
        if (pFormat != NULL)
                *pFormat = ma_format_unknown;
        if (pChannels != NULL)
                *pChannels = 0;
        if (pSampleRate != NULL)
                *pSampleRate = 0;
        if (pChannelMap != NULL)
                MA_ZERO_MEMORY(pChannelMap, sizeof(*pChannelMap) * channelMapCap);
        if (pWebm == NULL)
                return MA_INVALID_OPERATION;

        if (pFormat != NULL)
                *pFormat = pWebm->format;

#if !defined(MA_NO_WEBM)
        {
                if (pChannels != NULL)
                {
                        *pChannels = pWebm->channels;
                }

                if (pSampleRate != NULL)
                {
                        *pSampleRate = pWebm->sampleRate;
                }

                if (pChannelMap != NULL)
                {
                        if (pChannelMap != NULL)
                        {
                                ma_channel_map_init_standard(
                                    ma_standard_channel_map_vorbis,
                                    pChannelMap,
                                    channelMapCap,
                                    pWebm->channels);
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

MA_API ma_result ma_webm_get_cursor_in_pcm_frames(ma_webm *pWebm, ma_uint64 *pCursor)
{
        if (pCursor == NULL || pWebm == NULL)
        {
                return MA_INVALID_ARGS;
        }

#if !defined(MA_NO_WEBM)
        {
                *pCursor = pWebm->cursorInPCMFrames;
                return MA_SUCCESS;
        }
#else
        {
                MA_ASSERT(MA_FALSE);
                return MA_NOT_IMPLEMENTED;
        }
#endif
}

ma_uint64 calculate_length_in_pcm_frames(ma_webm *pWebm)
{
        uint64_t duration_ns = 0;
        if (nestegg_duration(pWebm->ctx, &duration_ns) == 0 && duration_ns > 0)
        {
                // NNanoseconds to PCM frames
                return (ma_uint64)((duration_ns * (uint64_t)pWebm->sampleRate) / 1000000000ull);
        }

        return 0;
}

MA_API ma_result ma_webm_get_length_in_pcm_frames(ma_webm *pWebm, ma_uint64 *pLength)
{
        if (pLength == NULL || pWebm == NULL)
        {
                return MA_INVALID_ARGS;
        }

#if !defined(MA_NO_WEBM)
        {
                if (pWebm->lengthInPCMFrames == 0)
                {
                        pWebm->lengthInPCMFrames = calculate_length_in_pcm_frames(pWebm);
                }
                *pLength = pWebm->lengthInPCMFrames;
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
