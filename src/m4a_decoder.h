
/*
This implements a data source that decodes m4a streams via FFmpeg

This object can be plugged into any `ma_data_source_*()` API and can also be used as a custom
decoding backend. See the custom_decoder example.

You need to include this file after miniaudio.h.
*/

#ifndef m4a_decoder_h
#define m4a_decoder_h

#ifdef __cplusplus
extern "C"
{
#endif
#include <string.h>
#include <miniaudio.h>
#include <libswresample/swresample.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <stdint.h>

        typedef struct
        {
                ma_data_source_base ds; /* The m4a decoder can be used independently as a data source. */
                ma_read_proc onRead;
                ma_seek_proc onSeek;
                ma_tell_proc onTell;
                void *pReadSeekTellUserData;
                ma_format format;
                FILE *mf;
                // FFmpeg related fields...
                AVCodecContext *codec_context;
                SwrContext *swr_ctx;
                AVFormatContext *format_context;
                ma_uint64 cursor;
                ma_uint32 sampleSize;
                int bitDepth;
        } m4a_decoder;

        MA_API ma_result m4a_decoder_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void *pReadSeekTellUserData, const ma_decoding_backend_config *pConfig, const ma_allocation_callbacks *pAllocationCallbacks, m4a_decoder *pM4a);
        MA_API ma_result m4a_decoder_init_file(const char *pFilePath, const ma_decoding_backend_config *pConfig, const ma_allocation_callbacks *pAllocationCallbacks, m4a_decoder *pM4a);
        MA_API void m4a_decoder_uninit(m4a_decoder *pM4a, const ma_allocation_callbacks *pAllocationCallbacks);
        MA_API ma_result m4a_decoder_read_pcm_frames(m4a_decoder *pM4a, void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead);
        MA_API ma_result m4a_decoder_seek_to_pcm_frame(m4a_decoder *pM4a, ma_uint64 frameIndex);
        MA_API ma_result m4a_decoder_get_data_format(m4a_decoder *pM4a, ma_format *pFormat, ma_uint32 *pChannels, ma_uint32 *pSampleRate, ma_channel *pChannelMap, size_t channelMapCap);
        MA_API ma_result m4a_decoder_get_cursor_in_pcm_frames(m4a_decoder *pM4a, ma_uint64 *pCursor);
        MA_API ma_result m4a_decoder_get_length_in_pcm_frames(m4a_decoder *pM4a, ma_uint64 *pLength);

#ifdef __cplusplus
}
#endif

#if defined(MINIAUDIO_IMPLEMENTATION) || defined(MA_IMPLEMENTATION)

#define MAX_CHANNELS 2
#define MAX_SAMPLES 4800 // Maximum expected frame size
#define MAX_SAMPLE_SIZE 4
static uint8_t leftoverBuffer[MAX_SAMPLES * MAX_CHANNELS * MAX_SAMPLE_SIZE];
// static float leftoverBuffer[MAX_SAMPLES * MAX_CHANNELS];

static int leftoverSampleCount = 0;

extern ma_result m4a_decoder_ds_get_data_format(ma_data_source *pDataSource, ma_format *pFormat, ma_uint32 *pChannels, ma_uint32 *pSampleRate, ma_channel *pChannelMap, size_t channelMapCap);

ma_result m4a_decoder_ds_read(ma_data_source *pDataSource, void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead)
{
        return m4a_decoder_read_pcm_frames((m4a_decoder *)pDataSource, pFramesOut, frameCount, pFramesRead);
}

ma_result m4a_decoder_ds_seek(ma_data_source *pDataSource, ma_uint64 frameIndex)
{
        return m4a_decoder_seek_to_pcm_frame((m4a_decoder *)pDataSource, frameIndex);
}

ma_result m4a_decoder_ds_get_data_format(ma_data_source *pDataSource, ma_format *pFormat, ma_uint32 *pChannels, ma_uint32 *pSampleRate, ma_channel *pChannelMap, size_t channelMapCap)
{
        return m4a_decoder_get_data_format((m4a_decoder *)pDataSource, pFormat, pChannels, pSampleRate, pChannelMap, channelMapCap);
}

ma_result m4a_decoder_ds_get_cursor(ma_data_source *pDataSource, ma_uint64 *pCursor)
{
        return m4a_decoder_get_cursor_in_pcm_frames((m4a_decoder *)pDataSource, pCursor);
}

ma_result m4a_decoder_ds_get_length(ma_data_source *pDataSource, ma_uint64 *pLength)
{
        return m4a_decoder_get_length_in_pcm_frames((m4a_decoder *)pDataSource, pLength);
}

ma_data_source_vtable g_m4a_decoder_ds_vtable =
    {
        m4a_decoder_ds_read,
        m4a_decoder_ds_seek,
        m4a_decoder_ds_get_data_format,
        m4a_decoder_ds_get_cursor,
        m4a_decoder_ds_get_length};

// Custom FFmpeg read function wrapper
int m4a_ffmpeg_read(void *opaque, uint8_t *buf, int buf_size)
{
        m4a_decoder *pM4a = (m4a_decoder *)opaque;
        size_t bytesRead = 0;

        ma_result result = pM4a->onRead(pM4a->pReadSeekTellUserData, buf, buf_size, &bytesRead);

        if (result == MA_SUCCESS)
        {
                return bytesRead;
        }
        else
        {
                // Translate MiniAudio error result to FFmpeg error codes
                switch (result)
                {
                case MA_IO_ERROR:
                        return AVERROR(EIO);
                case MA_INVALID_ARGS:
                        return AVERROR(EINVAL);

                default:
                        return AVERROR(EIO); // Default to a generic I/O error code
                }
        }
}

int64_t m4a_ffmpeg_seek(void *opaque, int64_t offset, int whence)
{
        m4a_decoder *pM4a = (m4a_decoder *)opaque;

        if (whence == AVSEEK_SIZE)
        {
                if (pM4a->onTell)
                {
                        ma_int64 fileSize;
                        ma_result result = pM4a->onTell(pM4a->pReadSeekTellUserData, &fileSize);
                        return result != MA_SUCCESS ? AVERROR(result) : fileSize;
                }
                else
                {
                        return AVERROR(ENOSYS);
                }
        }

        ma_seek_origin origin = ma_seek_origin_start;
        switch (whence)
        {
        case SEEK_SET:
                origin = ma_seek_origin_start;
                break;
        case SEEK_CUR:
                origin = ma_seek_origin_current;
                break;
        case SEEK_END:
                origin = ma_seek_origin_end;
                break;
        default:
                return AVERROR(EINVAL);
        }

        if (pM4a->onSeek)
        {
                ma_result result = pM4a->onSeek(pM4a->pReadSeekTellUserData, offset, origin);
                if (result != MA_SUCCESS)
                {
                        return AVERROR(result);
                }
                if (whence != SEEK_CUR || offset != 0)
                { // If the position really changed
                        ma_int64 newPosition;
                        result = pM4a->onTell(pM4a->pReadSeekTellUserData, &newPosition);
                        return result != MA_SUCCESS ? AVERROR(result) : newPosition;
                }
                else
                {
                        ma_int64 currentPosition;
                        result = pM4a->onTell(pM4a->pReadSeekTellUserData, &currentPosition);
                        return result != MA_SUCCESS ? AVERROR(result) : currentPosition;
                }
        }
        else
        {
                return AVERROR(ENOSYS);
        }
}

static ma_result m4a_decoder_init_internal(const ma_decoding_backend_config *pConfig, m4a_decoder *pM4a)
{
        if (pM4a == NULL)
        {
                return MA_INVALID_ARGS;
        }

        MA_ZERO_OBJECT(pM4a);
        pM4a->format = ma_format_f32;

        if (pConfig != NULL && (pConfig->preferredFormat == ma_format_f32 || pConfig->preferredFormat == ma_format_s16))
        {
                pM4a->format = pConfig->preferredFormat;
        }

        ma_data_source_config dataSourceConfig = ma_data_source_config_init();

        dataSourceConfig.vtable = &g_m4a_decoder_ds_vtable;

        ma_result result = ma_data_source_init(&dataSourceConfig, &pM4a->ds);
        if (result != MA_SUCCESS)
        {
                return result;
        }

        return MA_SUCCESS;
}

ma_format ffmpeg_to_mini_al_format(enum AVSampleFormat ffmpeg_sample_fmt)
{
        switch (ffmpeg_sample_fmt)
        {
        case AV_SAMPLE_FMT_FLTP:
        case AV_SAMPLE_FMT_FLT:
                return ma_format_f32;
        case AV_SAMPLE_FMT_S16:
        case AV_SAMPLE_FMT_S16P:
                return ma_format_s16;
        default:
                return ma_format_unknown;
        }
}

// Note: This isn't used by kew and is untested
MA_API ma_result m4a_decoder_init(
    ma_read_proc onRead,
    ma_seek_proc onSeek,
    ma_tell_proc onTell,
    void *pReadSeekTellUserData,
    const ma_decoding_backend_config *pConfig,
    const ma_allocation_callbacks *pAllocationCallbacks,
    m4a_decoder *pM4a)
{
        if (pM4a == NULL || onRead == NULL || onSeek == NULL)
        {
                return MA_INVALID_ARGS;
        }

        ma_result result = m4a_decoder_init_internal(pConfig, pM4a);
        if (result != MA_SUCCESS)
        {
                return result;
        }

        // Setup Custom I/O
        unsigned char *avio_ctx_buffer = NULL;
        size_t avio_ctx_buffer_size = 4096; // Use a buffer size that you find appropriate
        AVIOContext *avio_ctx = avio_alloc_context(
            avio_ctx_buffer,
            avio_ctx_buffer_size,
            0,
            pReadSeekTellUserData,
            m4a_ffmpeg_read,
            NULL,
            m4a_ffmpeg_seek);

        if (avio_ctx == NULL)
        {
                return MA_OUT_OF_MEMORY;
        }

        avio_ctx_buffer = (unsigned char *)av_malloc(avio_ctx_buffer_size);
        if (avio_ctx_buffer == NULL)
        {
                avio_context_free(&avio_ctx);
                return MA_OUT_OF_MEMORY;
        }

        // Initialize FFmpeg's AVFormatContext with the custom I/O context
        pM4a->format_context = avformat_alloc_context();
        if (pM4a->format_context == NULL)
        {
                av_free(avio_ctx_buffer);
                avio_context_free(&avio_ctx);
                return MA_OUT_OF_MEMORY;
        }
        pM4a->format_context->pb = avio_ctx;

        return MA_SUCCESS;
}

MA_API ma_result m4a_decoder_init_file(const char *pFilePath, const ma_decoding_backend_config *pConfig, const ma_allocation_callbacks *pAllocationCallbacks, m4a_decoder *pM4a)
{
        if (pFilePath == NULL || pM4a == NULL)
        {
                return MA_INVALID_ARGS;
        }

        ma_result result = m4a_decoder_init_internal(pConfig, pM4a);
        if (result != MA_SUCCESS)
        {
                return result;
        }

        // Initialize libavformat and libavcodec
        AVFormatContext *format_context = NULL;
        if (avformat_open_input(&format_context, pFilePath, NULL, NULL) != 0)
        {
                // Cleanup if initialization failed.
                return MA_INVALID_FILE;
        }

        if (avformat_find_stream_info(format_context, NULL) < 0)
        {
                // Cannot find stream info, clean up.
                avformat_close_input(&format_context);
                return MA_ERROR;
        }

        // Find the best audio stream.
        const AVCodec *decoder = NULL;
        int stream_index = av_find_best_stream(format_context, AVMEDIA_TYPE_AUDIO, -1, -1, &decoder, 0);
        if (stream_index < 0)
        {
                avformat_close_input(&format_context);
                return MA_ERROR;
        }

        // Initialize the codec context for the audio stream.
        AVStream *audio_stream = format_context->streams[stream_index];

        AVCodecContext *codec_context = avcodec_alloc_context3(decoder);
        if (!codec_context)
        {
                avformat_close_input(&format_context);
                return MA_OUT_OF_MEMORY;
        }

        if (avcodec_parameters_to_context(codec_context, audio_stream->codecpar) < 0)
        {
                avcodec_free_context(&codec_context);
                avformat_close_input(&format_context);
                return MA_ERROR;
        }

        if (avcodec_open2(codec_context, decoder, NULL) < 0)
        {
                avcodec_free_context(&codec_context);
                avformat_close_input(&format_context);
                return MA_ERROR;
        }

        // Assign the important objects to pM4a fields
        pM4a->codec_context = codec_context;

        switch (pM4a->codec_context->sample_fmt)
        {
        case AV_SAMPLE_FMT_S16:
        case AV_SAMPLE_FMT_S16P:
                pM4a->sampleSize = sizeof(int16_t); // 16-bit samples
                break;
        case AV_SAMPLE_FMT_FLTP:
        case AV_SAMPLE_FMT_FLT:
                pM4a->sampleSize = sizeof(float); // 32-bit float samples
                break;
        // ... handle other formats as needed ...
        default:
                pM4a->sampleSize = 0; // Unknown or unsupported format
                break;
        }

        pM4a->format_context = format_context;
        pM4a->mf = NULL; // This might be used for raw file operations which are not needed with FFmpeg.
        pM4a->format = ffmpeg_to_mini_al_format(pM4a->codec_context->sample_fmt);

        return MA_SUCCESS;
}

MA_API void m4a_decoder_uninit(m4a_decoder *pM4a, const ma_allocation_callbacks *pAllocationCallbacks)
{
        if (pM4a == NULL)
        {
                return;
        }

        (void)pAllocationCallbacks;

        if (pM4a->swr_ctx != NULL)
        {
                swr_free(&pM4a->swr_ctx);
        }

        if (pM4a->codec_context != NULL)
        {
                avcodec_free_context(&pM4a->codec_context);
        }

        if (pM4a->format_context != NULL)
        {
                avformat_close_input(&pM4a->format_context);
        }

        if (pM4a->mf != NULL)
        {
                fclose(pM4a->mf);
                pM4a->mf = NULL;
        }

        ma_data_source_uninit(&pM4a->ds);
}

ma_result m4a_decoder_read_pcm_frames(m4a_decoder *pM4a, void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead)
{
        if (pM4a == NULL || pM4a->onRead == NULL || pM4a->onSeek == NULL || pM4a->sampleSize == 0 || pFramesOut == NULL || frameCount == 0)
        {
                return MA_INVALID_ARGS;
        }

        ma_result result = MA_SUCCESS;
        ma_format format;
        ma_uint32 channels;
        ma_uint32 sampleRate;

        m4a_decoder_get_data_format(pM4a, &format, &channels, &sampleRate, NULL, 0);

        int stream_index = av_find_best_stream(pM4a->format_context, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
        if (stream_index < 0)
        {
                return MA_ERROR;
        }

        AVFrame *frame = av_frame_alloc();
        if (!frame)
        {
                return MA_ERROR;
        }

        AVPacket packet;
        ma_uint64 totalFramesProcessed = 0;

        if (leftoverSampleCount > 0)
        {
                int leftoverToProcess = (leftoverSampleCount < frameCount) ? leftoverSampleCount : frameCount;

                int leftoverBytes = leftoverToProcess * channels * pM4a->sampleSize;
                memcpy(pFramesOut, leftoverBuffer, leftoverBytes);
                totalFramesProcessed += leftoverToProcess;

                int bytesToShift = (leftoverSampleCount - leftoverToProcess) * channels * pM4a->sampleSize;
                uint8_t *shiftSource = leftoverBuffer + leftoverToProcess * channels * pM4a->sampleSize;
                memmove(leftoverBuffer, shiftSource, bytesToShift);
                leftoverSampleCount -= leftoverToProcess;
        }

        while (totalFramesProcessed < frameCount)
        {
                if (av_read_frame(pM4a->format_context, &packet) < 0)
                {
                        // Error in reading frame or EOF
                        result = MA_AT_END;
                        break;
                }

                if (packet.stream_index == stream_index)
                {
                        if (avcodec_send_packet(pM4a->codec_context, &packet) == 0)
                        {
                                while (totalFramesProcessed < frameCount && avcodec_receive_frame(pM4a->codec_context, frame) == 0)
                                {
                                        int samplesToProcess = (frame->nb_samples < frameCount - totalFramesProcessed) ? frame->nb_samples : frameCount - totalFramesProcessed;
                                        int outputBufferLen = samplesToProcess * channels * pM4a->sampleSize;

                                        uint8_t *output_buffer = (uint8_t *)malloc(outputBufferLen);
                                        if (!output_buffer)
                                        {
                                                av_frame_free(&frame);
                                                return MA_ERROR;
                                        }

                                        for (int i = 0; i < samplesToProcess; i++)
                                        {
                                                for (int c = 0; c < channels; c++)
                                                {
                                                        int byteOffset = (i * channels + c) * pM4a->sampleSize;

                                                        if (pM4a->format == ma_format_s16)
                                                        {
                                                                int16_t sample = ((int16_t *)frame->extended_data[c])[i];
                                                                memcpy(output_buffer + byteOffset, &sample, sizeof(int16_t));
                                                        }
                                                        else
                                                        {
                                                                float sample = ((float *)frame->extended_data[c])[i];
                                                                memcpy(output_buffer + byteOffset, &sample, sizeof(float));
                                                        }
                                                }
                                        }

                                        int current_frame_buffer_size = samplesToProcess * channels * pM4a->sampleSize;
                                        memcpy((uint8_t *)pFramesOut + totalFramesProcessed * channels * pM4a->sampleSize, output_buffer, current_frame_buffer_size);

                                        totalFramesProcessed += samplesToProcess;

                                        // Check if there are leftovers
                                        if (samplesToProcess < frame->nb_samples)
                                        {
                                                int remainingSamples = frame->nb_samples - samplesToProcess;

                                                for (int i = samplesToProcess; i < frame->nb_samples; i++)
                                                {
                                                        for (int c = 0; c < channels; c++)
                                                        {
                                                                int byteOffset = ((i - samplesToProcess) * channels + c) * pM4a->sampleSize;
                                                                memcpy(leftoverBuffer + byteOffset, (uint8_t *)frame->extended_data[c] + i * pM4a->sampleSize, pM4a->sampleSize);
                                                        }
                                                }
                                                leftoverSampleCount = remainingSamples;
                                        }

                                        free(output_buffer);
                                }
                        }
                }

                av_packet_unref(&packet);
        }

        av_frame_free(&frame);

        pM4a->cursor += totalFramesProcessed;

        if (pFramesRead != NULL)
        {
                *pFramesRead = totalFramesProcessed;
        }

        return result;
}

MA_API ma_result m4a_decoder_seek_to_pcm_frame(m4a_decoder *pM4a, ma_uint64 frameIndex)
{
        if (pM4a == NULL || pM4a->codec_context == NULL || pM4a->format_context == NULL)
        {
                return MA_INVALID_ARGS;
        }

        AVStream *stream = NULL;
        for (int i = 0; i < pM4a->format_context->nb_streams; i++)
        {
                if (pM4a->format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
                {
                        stream = pM4a->format_context->streams[i];
                        break;
                }
        }

        if (stream == NULL)
        {
                return MA_ERROR;
        }

        // Convert frame index to the stream's time base.
        int64_t timestamp = av_rescale_q(frameIndex,
                                         (AVRational){1, pM4a->codec_context->sample_rate},
                                         stream->time_base);

        if (av_seek_frame(pM4a->format_context, stream->index, timestamp, AVSEEK_FLAG_BACKWARD) < 0)
        {
                return MA_ERROR;
        }

        // After seeking, we must clear the codec's internal buffer.
        avcodec_flush_buffers(pM4a->codec_context);

        return MA_SUCCESS;
}

MA_API ma_result m4a_decoder_get_data_format(
    m4a_decoder *pM4a,
    ma_format *pFormat,
    ma_uint32 *pChannels,
    ma_uint32 *pSampleRate,
    ma_channel *pChannelMap,
    size_t channelMapCap)
{
        if (pFormat != NULL)
        {
                *pFormat = ma_format_unknown;
        }
        if (pChannels != NULL)
        {
                *pChannels = 0;
        }
        if (pSampleRate != NULL)
        {
                *pSampleRate = 0;
        }
        if (pChannelMap != NULL)
        {
                MA_ZERO_MEMORY(pChannelMap, sizeof(*pChannelMap) * channelMapCap);
        }

        if (pM4a == NULL || pM4a->codec_context == NULL)
        {
                return MA_INVALID_OPERATION;
        }

        if (pFormat != NULL)
        {
                *pFormat = ffmpeg_to_mini_al_format(pM4a->codec_context->sample_fmt);
        }

        if (pChannels != NULL)
        {
                *pChannels = pM4a->codec_context->ch_layout.nb_channels;
        }

        if (pSampleRate != NULL)
        {
                *pSampleRate = pM4a->codec_context->sample_rate;
        }

        if (pChannelMap != NULL)
        {
                ma_channel_map_init_standard(ma_standard_channel_map_microsoft, pChannelMap, channelMapCap, *pChannels);
        }

        return MA_SUCCESS;
}

MA_API ma_result m4a_decoder_get_cursor_in_pcm_frames(m4a_decoder *pM4a, ma_uint64 *pCursor)
{
        if (pCursor == NULL)
        {
                return MA_INVALID_ARGS;
        }

        *pCursor = 0; /* Safety. */

        if (pM4a == NULL || pM4a->format_context == NULL || pM4a->codec_context == NULL)
        {
                return MA_INVALID_ARGS;
        }

        *pCursor = pM4a->cursor;

        return MA_SUCCESS;
}

MA_API ma_result m4a_decoder_get_length_in_pcm_frames(m4a_decoder *pM4a, ma_uint64 *pLength)
{
        if (pLength == NULL)
        {
                return MA_INVALID_ARGS;
        }

        *pLength = 0; // Safety.

        if (pM4a == NULL || pM4a->format_context == NULL || pM4a->codec_context == NULL)
        {
                return MA_INVALID_ARGS;
        }

        AVStream *audio_stream = NULL;
        for (int i = 0; i < pM4a->format_context->nb_streams; i++)
        {
                if (pM4a->format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
                {
                        audio_stream = pM4a->format_context->streams[i];
                        break;
                }
        }

        if (audio_stream == NULL)
        {
                return MA_ERROR;
        }

        // Use duration and time base to calculate total number of frames
        if (audio_stream->duration != AV_NOPTS_VALUE)
        {
                int64_t duration_ts = audio_stream->duration;
                AVRational time_base = audio_stream->time_base;
                AVRational target_time_base = {1, pM4a->codec_context->sample_rate};
                *pLength = av_rescale_q(duration_ts, time_base, target_time_base);
                return MA_SUCCESS;
        }

        return MA_ERROR;
}
#endif
#endif
