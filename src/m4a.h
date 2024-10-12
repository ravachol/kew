/*
This implements a data source that decodes m4a streams via FFmpeg

This object can be plugged into any `ma_data_source_*()` API and can also be used as a custom
decoding backend. See the custom_decoder example.

You need to include this file after miniaudio.h.
*/

#ifndef M4A_H
#define M4A_H
#ifdef __cplusplus
extern "C"
{
#endif
#include <miniaudio.h>
#include "neaacdec.h"
#include "../include/minimp4/minimp4.h" // Adjust the path as necessary
#include <stdint.h>
#include <string.h>

        typedef struct
        {
                ma_data_source_base ds; /* The m4a decoder can be used independently as a data source. */
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
                int bitDepth;
                ma_uint32 sampleRate;
                ma_uint32 channels;

                // minimp4 fields...
                MP4D_demux_t mp4;
                MP4D_track_t *track;
                int32_t audio_track_index;
                uint32_t current_sample;
                uint32_t total_samples;

                // For m4a_decoder_init_file
                FILE *file;

                ma_uint64 cursor;
        } m4a_decoder;

        MA_API ma_result m4a_decoder_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void *pReadSeekTellUserData, const ma_decoding_backend_config *pConfig, const ma_allocation_callbacks *pAllocationCallbacks, m4a_decoder *pM4a);
        MA_API ma_result m4a_decoder_init_file(const char *pFilePath, const ma_decoding_backend_config *pConfig, const ma_allocation_callbacks *pAllocationCallbacks, m4a_decoder *pM4a);
        MA_API void m4a_decoder_uninit(m4a_decoder *pM4a, const ma_allocation_callbacks *pAllocationCallbacks);
        MA_API ma_result m4a_decoder_read_pcm_frames(m4a_decoder *pM4a, void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead);
        MA_API ma_result m4a_decoder_seek_to_pcm_frame(m4a_decoder *pM4a, ma_uint64 frameIndex);
        MA_API ma_result m4a_decoder_get_data_format(m4a_decoder *pM4a, ma_format *pFormat, ma_uint32 *pChannels, ma_uint32 *pSampleRate, ma_channel *pChannelMap, size_t channelMapCap);
        MA_API ma_result m4a_decoder_get_cursor_in_pcm_frames(m4a_decoder *pM4a, ma_uint64 *pCursor);
        MA_API ma_result m4a_decoder_get_length_in_pcm_frames(m4a_decoder *pM4a, ma_uint64 *pLength);

        extern ma_result m4a_decoder_ds_get_data_format(ma_data_source *pDataSource, ma_format *pFormat, ma_uint32 *pChannels, ma_uint32 *pSampleRate, ma_channel *pChannelMap, size_t channelMapCap);

        extern ma_result m4a_decoder_ds_read(ma_data_source *pDataSource, void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead);

        extern ma_result m4a_decoder_ds_seek(ma_data_source *pDataSource, ma_uint64 frameIndex);

        extern ma_result m4a_decoder_ds_get_data_format(ma_data_source *pDataSource, ma_format *pFormat, ma_uint32 *pChannels, ma_uint32 *pSampleRate, ma_channel *pChannelMap, size_t channelMapCap);

        extern ma_result m4a_decoder_ds_get_cursor(ma_data_source *pDataSource, ma_uint64 *pCursor);

        extern ma_result m4a_decoder_ds_get_length(ma_data_source *pDataSource, ma_uint64 *pLength);

#if defined(MINIAUDIO_IMPLEMENTATION) || defined(MA_IMPLEMENTATION)

#define MAX_CHANNELS 2
#define MAX_SAMPLES 4800 // Maximum expected frame size
#define MAX_SAMPLE_SIZE 4
        static uint8_t leftoverBuffer[MAX_SAMPLES * MAX_CHANNELS * MAX_SAMPLE_SIZE];

        static ma_uint64 leftoverSampleCount = 0;

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
                m4a_decoder_ds_get_length,
                NULL,
                (ma_uint64)0};

        static ma_result file_on_read(void *pUserData, void *pBufferOut, size_t bytesToRead, size_t *pBytesRead)
        {
                FILE *fp = (FILE *)pUserData;
                size_t bytesRead = fread(pBufferOut, 1, bytesToRead, fp);
                if (bytesRead < bytesToRead && ferror(fp))
                {
                        return MA_ERROR;
                }
                if (pBytesRead)
                {
                        *pBytesRead = bytesRead;
                }
                return MA_SUCCESS;
        }

        static ma_result file_on_seek(void *pUserData, ma_int64 offset, ma_seek_origin origin)
        {
                FILE *fp = (FILE *)pUserData;
                int whence = (origin == ma_seek_origin_start) ? SEEK_SET : SEEK_CUR;
                if (fseeko(fp, offset, whence) != 0)
                {
                        return MA_ERROR;
                }
                return MA_SUCCESS;
        }

        // static ma_result file_on_tell(void *pUserData, ma_int64 *pCursor)
        // {
        //         FILE *fp = (FILE *)pUserData;
        //         long pos = ftell(fp);
        //         if (pos < 0)
        //         {
        //                 return MA_ERROR;
        //         }
        //         if (pCursor)
        //         {
        //                 *pCursor = (ma_int64)pos;
        //         }
        //         return MA_SUCCESS;
        // }

        static int minimp4_read_callback(int64_t offset, void *buffer, size_t size, void *token)
        {
                m4a_decoder *pM4a = (m4a_decoder *)token;

                // Cast int64_t to ma_int64 for onSeek
                ma_int64 ma_offset = (ma_int64)offset;
                if (file_on_seek(pM4a->file, ma_offset, ma_seek_origin_start) != MA_SUCCESS)
                {
                        return 1; // Error
                }

                size_t bytesRead = 0;
                if (file_on_read(pM4a->file, buffer, size, &bytesRead) != MA_SUCCESS || bytesRead != size)
                {
                        return 1; // Error
                }

                return 0; // Success
        }

        int64_t minimp4_seek_callback(void *user_data, int64_t offset)
        {
                m4a_decoder *pM4a = (m4a_decoder *)user_data;
                ma_result result = file_on_seek(pM4a->file, offset, ma_seek_origin_start);
                if (result != MA_SUCCESS)
                {
                        return -1; // Signal error
                }
                return offset; // Return the new position if possible
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
                (void)pAllocationCallbacks;

                if (pM4a == NULL || onRead == NULL || onSeek == NULL || onTell == NULL)
                {
                        return MA_INVALID_ARGS;
                }

                ma_result result = m4a_decoder_init_internal(pConfig, pM4a);
                if (result != MA_SUCCESS)
                {
                        return result;
                }

                // Store the custom read, seek, and tell functions
                pM4a->pReadSeekTellUserData = pReadSeekTellUserData;

                // Get the size of the data source
                ma_int64 currentPos = 0;
                if (pM4a->onTell(pM4a->pReadSeekTellUserData, &currentPos) != MA_SUCCESS)
                {
                        return MA_ERROR;
                }

                if (pM4a->onSeek(pM4a->pReadSeekTellUserData, 0, ma_seek_origin_end) != MA_SUCCESS)
                {
                        return MA_ERROR;
                }

                ma_int64 fileSize = 0;
                if (pM4a->onTell(pM4a->pReadSeekTellUserData, &fileSize) != MA_SUCCESS)
                {
                        return MA_ERROR;
                }

                // Seek back to original position
                if (pM4a->onSeek(pM4a->pReadSeekTellUserData, currentPos, ma_seek_origin_start) != MA_SUCCESS)
                {
                        return MA_ERROR;
                }

                // Initialize minimp4 with custom read_callback
                if (MP4D_open(&pM4a->mp4, minimp4_read_callback, pM4a, fileSize) != 0)
                {
                        return MA_ERROR;
                }

                // Find the audio track
                pM4a->audio_track_index = -1;
                for (unsigned int i = 0; i < pM4a->mp4.track_count; i++)
                {
                        MP4D_track_t *track = &pM4a->mp4.track[i];
                        if (track->handler_type == MP4D_HANDLER_TYPE_SOUN)
                        {
                                pM4a->audio_track_index = i;
                                pM4a->track = track;
                                break;
                        }
                }

                if (pM4a->audio_track_index == -1)
                {
                        // No audio track found
                        MP4D_close(&pM4a->mp4);
                        return MA_ERROR;
                }

                pM4a->current_sample = 0;
                pM4a->total_samples = pM4a->track->sample_count;

                // Initialize faad2 decoder
                pM4a->hDecoder = NeAACDecOpen();

                // Extract the decoder configuration
                const uint8_t *decoder_config = pM4a->track->dsi;
                uint32_t decoder_config_len = pM4a->track->dsi_bytes;

                unsigned long sampleRate;
                unsigned char channels;

                if (NeAACDecInit2(pM4a->hDecoder, (unsigned char *)decoder_config, decoder_config_len, &sampleRate, &channels) < 0)
                {
                        // Error initializing decoder
                        NeAACDecClose(pM4a->hDecoder);
                        MP4D_close(&pM4a->mp4);
                        return MA_ERROR;
                }

                // Configure output format
                NeAACDecConfigurationPtr config = NeAACDecGetCurrentConfiguration(pM4a->hDecoder);
                if (pM4a->format == ma_format_s16)
                {
                        config->outputFormat = FAAD_FMT_16BIT;
                        pM4a->sampleSize = sizeof(int16_t);
                        pM4a->bitDepth = 16;
                }
                else if (pM4a->format == ma_format_f32)
                {
                        config->outputFormat = FAAD_FMT_FLOAT;
                        pM4a->sampleSize = sizeof(float);
                        pM4a->bitDepth = 32;
                }
                else
                {
                        // Unsupported format
                        NeAACDecClose(pM4a->hDecoder);
                        MP4D_close(&pM4a->mp4);
                        return MA_ERROR;
                }
                NeAACDecSetConfiguration(pM4a->hDecoder, config);

                // Initialize other fields
                leftoverSampleCount = 0;
                pM4a->cursor = 0;

                return MA_SUCCESS;
        }

        MA_API ma_result m4a_decoder_init_file(
            const char *pFilePath,
            const ma_decoding_backend_config *pConfig,
            const ma_allocation_callbacks *pAllocationCallbacks,
            m4a_decoder *pM4a)
        {
                (void)pAllocationCallbacks;

                if (pFilePath == NULL || pM4a == NULL)
                {
                        return MA_INVALID_ARGS;
                }

                ma_result result = m4a_decoder_init_internal(pConfig, pM4a);
                if (result != MA_SUCCESS)
                {
                        return result;
                }

                FILE *fp = fopen(pFilePath, "rb");
                if (fp == NULL)
                {
                        return MA_INVALID_FILE;
                }

                // Get the file size
                if (fseeko(fp, 0, SEEK_END) != 0)
                {
                        fclose(fp);
                        return MA_ERROR;
                }

                ma_int64 fileSize = ftello(fp);
                if (fileSize < 0)
                {
                        fclose(fp);
                        return MA_ERROR;
                }

                if (fseeko(fp, 0, SEEK_SET) != 0)
                {
                        fclose(fp);
                        return MA_ERROR;
                }

                // Store the FILE pointer in the decoder struct
                pM4a->file = fp;

                // Initialize minimp4 with the custom read callback, using mp4 as a struct member
                if (MP4D_open(&pM4a->mp4, minimp4_read_callback, pM4a, fileSize) != 1)
                {
                        fclose(fp);
                        return MA_ERROR;
                }

                // Find the audio track
                pM4a->audio_track_index = -1;
                for (unsigned int i = 0; i < pM4a->mp4.track_count; i++)
                {
                        MP4D_track_t *track = &pM4a->mp4.track[i];
                        if (track->handler_type == MP4D_HANDLER_TYPE_SOUN)
                        {
                                pM4a->audio_track_index = i;
                                pM4a->track = track;
                                break;
                        }
                }

                if (pM4a->audio_track_index == -1)
                {
                        // No audio track found
                        MP4D_close(&pM4a->mp4);
                        fclose(fp);
                        return MA_ERROR;
                }

                pM4a->current_sample = 0;
                pM4a->total_samples = pM4a->track->sample_count;

                // Initialize faad2 decoder
                pM4a->hDecoder = NeAACDecOpen();

                // Extract the decoder configuration
                const uint8_t *decoder_config = pM4a->track->dsi;
                uint32_t decoder_config_len = pM4a->track->dsi_bytes;

                unsigned long sampleRate;
                unsigned char channels;

                if (NeAACDecInit2(pM4a->hDecoder, (unsigned char *)decoder_config, decoder_config_len, &sampleRate, &channels) < 0)
                {
                        // Error initializing decoder
                        NeAACDecClose(pM4a->hDecoder);
                        MP4D_close(&pM4a->mp4);
                        fclose(fp);
                        return MA_ERROR;
                }

                pM4a->sampleRate = (ma_uint32)sampleRate;
                pM4a->channels = (ma_uint32)channels;

                // Configure output format
                NeAACDecConfigurationPtr config_ptr = NeAACDecGetCurrentConfiguration(pM4a->hDecoder);
                if (pM4a->format == ma_format_s16)
                {
                        config_ptr->outputFormat = FAAD_FMT_16BIT;
                        pM4a->sampleSize = sizeof(int16_t);
                        pM4a->bitDepth = 16;
                }
                else if (pM4a->format == ma_format_f32)
                {
                        config_ptr->outputFormat = FAAD_FMT_FLOAT;
                        pM4a->sampleSize = sizeof(float);
                        pM4a->bitDepth = 32;
                }
                else
                {
                        // Unsupported format
                        NeAACDecClose(pM4a->hDecoder);
                        MP4D_close(&pM4a->mp4);
                        fclose(fp);
                        return MA_ERROR;
                }
                NeAACDecSetConfiguration(pM4a->hDecoder, config_ptr);

                // Initialize other fields
                leftoverSampleCount = 0;
                pM4a->cursor = 0;

                return MA_SUCCESS;
        }

        MA_API void m4a_decoder_uninit(m4a_decoder *pM4a, const ma_allocation_callbacks *pAllocationCallbacks)
        {
                (void)pAllocationCallbacks;

                if (pM4a == NULL)
                {
                        return;
                }

                // Close faad2 decoder
                if (pM4a->hDecoder)
                {
                        NeAACDecClose(pM4a->hDecoder);
                        pM4a->hDecoder = NULL;
                }

                // Close the minimp4 demuxer (no need to free `mp4` since it's not dynamically allocated)
                MP4D_close(&pM4a->mp4);

                // Close the file
                if (pM4a->file)
                {
                        fclose(pM4a->file);
                        pM4a->file = NULL;
                }
        }

        MA_API ma_result m4a_decoder_read_pcm_frames(
            m4a_decoder *pM4a,
            void *pFramesOut,
            ma_uint64 frameCount,
            ma_uint64 *pFramesRead)
        {
                if (pM4a == NULL || pFramesOut == NULL || frameCount == 0)
                {
                        return MA_INVALID_ARGS;
                }

                ma_result result = MA_SUCCESS;
                ma_uint32 channels = pM4a->channels;
                ma_uint32 sampleSize = pM4a->sampleSize;
                ma_uint64 totalFramesProcessed = 0;

                // Handle any leftover samples from previous call using the global/static leftover buffer
                if (leftoverSampleCount > 0)
                {
                        ma_uint64 leftoverToProcess = (leftoverSampleCount < frameCount) ? leftoverSampleCount : frameCount;
                        ma_uint64 leftoverBytes = leftoverToProcess * channels * sampleSize;

                        memcpy(pFramesOut, leftoverBuffer, leftoverBytes);
                        totalFramesProcessed += leftoverToProcess;

                        // Shift the leftover buffer
                        ma_uint64 samplesLeft = leftoverSampleCount - leftoverToProcess;
                        if (samplesLeft > 0)
                        {
                                memmove(leftoverBuffer, leftoverBuffer + leftoverBytes, samplesLeft * channels * sampleSize);
                        }
                        leftoverSampleCount = samplesLeft;
                }

                while (totalFramesProcessed < frameCount)
                {
                        if (pM4a->current_sample >= pM4a->total_samples)
                        {
                                result = MA_AT_END;
                                break; // No more samples
                        }

                        unsigned int frame_bytes = 0;
                        unsigned int timestamp = 0;
                        unsigned int duration = 0;

                        // Get the sample offset and size using minimp4
                        ma_int64 sample_offset = MP4D_frame_offset(
                            &pM4a->mp4,
                            pM4a->audio_track_index,
                            pM4a->current_sample,
                            &frame_bytes,
                            &timestamp,
                            &duration);

                        if (sample_offset == (ma_int64)(MP4D_file_offset_t)-1 || frame_bytes == 0)
                        {
                                // Error getting sample info
                                result = MA_ERROR;
                                break;
                        }

                        // Allocate buffer for the sample data
                        uint8_t *sample_data = (uint8_t *)malloc(frame_bytes);
                        if (sample_data == NULL)
                        {
                                result = MA_OUT_OF_MEMORY;
                                break;
                        }

                        // Read the sample data directly from the file
                        size_t bytesRead = 0;
                        if (file_on_read(pM4a->file, sample_data, frame_bytes, &bytesRead) != MA_SUCCESS || bytesRead != frame_bytes)
                        {
                                free(sample_data);
                                result = MA_ERROR;
                                break;
                        }

                        pM4a->current_sample++;

                        // Decode the AAC frame using faad2
                        void *decodedData = NeAACDecDecode(pM4a->hDecoder, &(pM4a->frameInfo), sample_data, frame_bytes);

                        free(sample_data); // Free the sample data buffer

                        if (pM4a->frameInfo.error > 0)
                        {
                                // Error in decoding, skip to the next frame.
                                continue;
                        }

                        unsigned long samplesDecoded = pM4a->frameInfo.samples; // Total samples decoded (channels * frames)
                        ma_uint64 framesDecoded = samplesDecoded / channels;

                        // Calculate how many frames we can process in this call
                        ma_uint64 framesNeeded = frameCount - totalFramesProcessed;
                        ma_uint64 framesToCopy = (framesDecoded < framesNeeded) ? framesDecoded : framesNeeded;
                        ma_uint64 bytesToCopy = framesToCopy * channels * sampleSize;

                        memcpy((uint8_t *)pFramesOut + totalFramesProcessed * channels * sampleSize, decodedData, bytesToCopy);
                        totalFramesProcessed += framesToCopy;

                        // Handle leftover frames using the global/static leftover buffer
                        if (framesToCopy < framesDecoded)
                        {
                                // There are leftover frames
                                leftoverSampleCount = framesDecoded - framesToCopy;
                                ma_uint64 leftoverBytes = leftoverSampleCount * channels * sampleSize;

                                if (leftoverBytes > sizeof(leftoverBuffer))
                                {
                                        // Safety check to avoid overflow in the buffer.
                                        leftoverSampleCount = sizeof(leftoverBuffer) / (channels * sampleSize);
                                        leftoverBytes = leftoverSampleCount * channels * sampleSize;
                                }

                                memcpy(leftoverBuffer, (uint8_t *)decodedData + bytesToCopy, leftoverBytes);
                        }
                        else
                        {
                                leftoverSampleCount = 0;
                        }
                }

                pM4a->cursor += totalFramesProcessed;

                if (pFramesRead != NULL)
                {
                        *pFramesRead = totalFramesProcessed;
                }

                return (totalFramesProcessed > 0) ? MA_SUCCESS : result;
        }

        MA_API ma_result m4a_decoder_seek_to_pcm_frame(m4a_decoder *pM4a, ma_uint64 frameIndex)
        {
                if (pM4a == NULL || pM4a->track == NULL || pM4a->hDecoder == NULL)
                {
                        return MA_INVALID_ARGS;
                }

                // Ensure the frameIndex does not exceed the total number of samples.
                if (frameIndex >= pM4a->total_samples)
                {
                        return MA_INVALID_ARGS;
                }

                // Find the sample in the track using minimp4.
                // Convert the frame index to the sample index, as each frame corresponds to one sample in this context.
                pM4a->current_sample = (uint32_t)frameIndex;

                // Calculate the offset of the sample in the file using MP4D_frame_offset.
                unsigned int frame_bytes = 0;
                unsigned int timestamp = 0;
                unsigned int duration = 0;

                ma_int64 sample_offset = MP4D_frame_offset(
                    &pM4a->mp4,
                    pM4a->audio_track_index,
                    pM4a->current_sample,
                    &frame_bytes,
                    &timestamp,
                    &duration);

                if (sample_offset == (ma_int64)(MP4D_file_offset_t)-1 || frame_bytes == 0)
                {
                        return MA_ERROR;
                }

                // Seek to the calculated sample offset in the file.
                if (file_on_seek(pM4a->file, sample_offset, ma_seek_origin_start) != MA_SUCCESS)
                {
                        return MA_ERROR;
                }

                // After seeking, reset the faad2 decoder state to start decoding from the new frame.
                NeAACDecPostSeekReset(pM4a->hDecoder, (long)pM4a->current_sample);

                // Clear leftover samples since we've moved to a new position.
                leftoverSampleCount = 0;

                // Update the decoder's cursor to the new frame position.
                pM4a->cursor = frameIndex;

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

                if (pM4a == NULL || pM4a->track == NULL)
                {
                        return MA_INVALID_OPERATION;
                }

                // Set the format based on the faad2 output configuration
                if (pFormat != NULL)
                {
                        if (pM4a->format == ma_format_s16)
                        {
                                *pFormat = ma_format_s16;
                        }
                        else if (pM4a->format == ma_format_f32)
                        {
                                *pFormat = ma_format_f32;
                        }
                        else
                        {
                                return MA_INVALID_OPERATION;
                        }
                }

                // Set the channel count from faad2
                if (pChannels != NULL)
                {
                        *pChannels = pM4a->channels;
                }

                // Set the sample rate from the faad2 decoder configuration
                if (pSampleRate != NULL)
                {
                        *pSampleRate = pM4a->sampleRate;
                }

                // Set a standard channel map if requested
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

                if (pM4a == NULL)
                {
                        return MA_INVALID_ARGS;
                }

                *pCursor = pM4a->cursor;

                return MA_SUCCESS;
        }

        // Note: This returns an approximation
        MA_API ma_result m4a_decoder_get_length_in_pcm_frames(m4a_decoder *pM4a, ma_uint64 *pLength)
        {
                if (pLength == NULL)
                {
                        return MA_INVALID_ARGS;
                }

                *pLength = 0; // Safety.

                if (pM4a == NULL || pM4a->track == NULL)
                {
                        return MA_INVALID_ARGS;
                }

                // Calculate the length in PCM frames using the total number of samples and the sample rate.
                if (pM4a->total_samples > 0 && pM4a->sampleRate > 0)
                {
                        *pLength = (ma_uint64)pM4a->total_samples;
                        return MA_SUCCESS;
                }

                return MA_ERROR;
        }
#endif

#ifdef __cplusplus
}
#endif
#endif
