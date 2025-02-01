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
                double duration;
                unsigned long totalFrames;

                // tells the program this is a raw AAC file
                int isRawAAC;

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

        double calculate_aac_duration(FILE *fp, unsigned long sampleRate, unsigned long *totalFrames)
        {
                if (fp == NULL || sampleRate == 0 || totalFrames == NULL)
                {
                        return -1.0;
                }

                unsigned char buffer[7];
                unsigned long fileSize = 0;
                *totalFrames = 0;

                // Get file size
                fseek(fp, 0, SEEK_END);
                fileSize = ftell(fp);
                fseek(fp, 0, SEEK_SET);

                // Loop to count frames
                while (ftell(fp) < (long)(fileSize - 7)) // Ensure at least an ADTS header remains
                {
                        // Read header
                        if (fread(buffer, 1, 7, fp) < 7)
                                break;

                        // Extract frame size
                        unsigned int frameSize = ((buffer[3] & 0x03) << 11) | ((buffer[4] & 0xFF) << 3) | ((buffer[5] & 0xE0) >> 5);

                        if (frameSize <= 7)
                                break;

                        // Skip to next frame
                        fseek(fp, frameSize - 7, SEEK_CUR);

                        (*totalFrames)++;
                }

                // Compute duration using: duration = (totalFrames * 1024) / sampleRate
                double duration = (double)(*totalFrames * 1024) / sampleRate;

                fseek(fp, 0, SEEK_SET);

                return duration;
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

                // Try to detect the file format (ADTS, MP4, LATM, etc.)
                unsigned char buffer[7];
                size_t bytesRead = fread(buffer, 1, sizeof(buffer), fp);

                // Check for ADTS header
                if (bytesRead >= 7 && buffer[0] == 0xFF && (buffer[1] & 0xF0) == 0xF0)
                {
                        pM4a->isRawAAC = MA_TRUE;
                }
                else
                {
                        // Check if it's an MP4 file (using MP4D_open or similar)
                        if (MP4D_open(&pM4a->mp4, minimp4_read_callback, pM4a, fileSize) == 1)
                        {
                                pM4a->isRawAAC = MA_FALSE; // It's an MP4 container
                        }
                        else
                        {
                                fclose(fp);
                                return MA_ERROR; // Unknown format
                        }
                }

                if (pM4a->isRawAAC)
                {
                        // Raw AAC handling

                        // Extract the frame size from the ADTS header
                        unsigned int frameSize = ((buffer[3] & 0x03) << 11) | ((buffer[4] & 0xFF) << 3) | ((buffer[5] & 0xE0) >> 5);

                        if (frameSize <= 7)
                        {
                                fclose(fp);
                                return MA_ERROR; // Invalid frame size
                        }

                        unsigned char *frameData = malloc(frameSize);
                        if (frameData == NULL)
                        {
                                fclose(fp);
                                return MA_ERROR; // Memory allocation failed
                        }

                        // The first 7 bytes are already in the buffer, so copy them to frameData
                        memcpy(frameData, buffer, 7);

                        // Read the rest of the frame (audio data)
                        size_t remainingBytes = frameSize - 7;
                        size_t additionalBytesRead = fread(frameData + 7, 1, remainingBytes, fp);
                        if (additionalBytesRead < remainingBytes)
                        {
                                free(frameData);
                                fclose(fp);
                                return MA_ERROR; // Failed to read the full frame
                        }

                        // Allocate decoder config
                        unsigned char *decoder_config = malloc(2);
                        if (decoder_config == NULL)
                        {
                                return MA_OUT_OF_MEMORY;
                        }

                        unsigned long decoder_config_size = 2;

                        decoder_config[0] = ((buffer[2] & 0xC0) >> 6) + 1; // Object type
                        decoder_config[0] |= ((buffer[2] & 0x3C) >> 2) << 2;

                        decoder_config[1] = ((buffer[2] & 0x07) << 1) | ((buffer[3] & 0x80) >> 7); // Channels and sampleRate
                        decoder_config[1] <<= 4;                                                   // Shift to upper 4 bits

                        unsigned char objectType = decoder_config[0];
                        if (objectType == 5 || objectType >= 29)
                        {
                                printf("File is encoded with HE-AAC which is not supported\n");
                                free(frameData);
                                free(decoder_config);
                                fclose(fp);
                                return MA_ERROR;
                        }

                        unsigned long sampleRate = 0;
                        unsigned char channels = 0;

                        pM4a->hDecoder = NeAACDecOpen();

                        int initResult = NeAACDecInit2(pM4a->hDecoder, (unsigned char *)decoder_config, decoder_config_size, &sampleRate, &channels);
                        if (initResult < 0)
                        {
                                printf("Error initializing decoder. Code: %d\n", initResult);
                                free(frameData);
                                free(decoder_config);
                                NeAACDecClose(pM4a->hDecoder);
                                fclose(fp);
                                return MA_ERROR;
                        }

                        free(decoder_config);

                        // Check if the sampleRate and channels are correctly initialized
                        if (sampleRate == 0 || channels == 0)
                        {
                                printf("Error: Invalid sample rate or channel count.\n");
                                free(frameData);
                                NeAACDecClose(pM4a->hDecoder);
                                fclose(fp);
                                return MA_ERROR;
                        }

                        pM4a->sampleRate = (ma_uint32)sampleRate;
                        pM4a->channels = (ma_uint32)channels;

                        pM4a->duration = calculate_aac_duration(fp, pM4a->sampleRate, &pM4a->totalFrames);

                        // Clean up the frame data after processing
                        free(frameData);

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
                                fclose(fp);
                                return MA_ERROR;
                        }
                        NeAACDecSetConfiguration(pM4a->hDecoder, config_ptr);

                        // Initialize other fields
                        leftoverSampleCount = 0;
                        pM4a->cursor = 0;

                        fseek(pM4a->file, 0, SEEK_SET);

                        return MA_SUCCESS;
                }
                else
                {
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

                        // M4A (MP4-wrapped AAC) handling
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

                if (!pM4a->isRawAAC)
                {
                        // Close the MiniMP4 demuxer (only for M4A)
                        MP4D_close(&pM4a->mp4);
                }

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
                        if (pM4a->isRawAAC) // Handling for raw AAC files
                        {
                                unsigned int headerSize = 7;
                                uint8_t buffer[headerSize];

                                if (fread(buffer, 1, headerSize, pM4a->file) != headerSize)
                                {
                                        result = MA_ERROR;
                                        break;
                                }

                                unsigned int frame_bytes = ((buffer[3] & 0x03) << 11) | ((buffer[4] & 0xFF) << 3) | ((buffer[5] & 0xE0) >> 5);

                                if (frame_bytes < headerSize || frame_bytes > 8192) // Safety limit
                                {
                                        result = MA_ERROR;
                                        break;
                                }

                                // Allocate memory for the frame
                                unsigned char *sample_data = (unsigned char *)malloc(frame_bytes);
                                if (!sample_data)
                                {
                                        result = MA_OUT_OF_MEMORY;
                                        break;
                                }

                                // Copy the header to the sample_data buffer
                                memcpy(sample_data, buffer, headerSize);

                                // Read the rest of the frame (audio data)
                                size_t remaining_bytes = frame_bytes - headerSize;
                                size_t additionalBytesRead = fread(sample_data + headerSize, 1, remaining_bytes, pM4a->file);

                                if (additionalBytesRead < remaining_bytes)
                                {
                                        free(sample_data);
                                        result = MA_ERROR;
                                        break; // Failed to read full frame
                                }

                                pM4a->current_sample++;

                                // Decode the AAC frame using faad2
                                void *decodedData = NeAACDecDecode(pM4a->hDecoder, &(pM4a->frameInfo), sample_data + 7, frame_bytes - 7);
                                free(sample_data);

                                if (pM4a->frameInfo.error > 0)
                                {
                                        // Error in decoding, skip to the next frame.
                                        continue;
                                }

                                // Remove support for HE-AAC components (SBR or PS)
                                if (pM4a->frameInfo.sbr || pM4a->frameInfo.ps)
                                {
                                        printf("File is encoded with HE-AAC which is not supported\n");
                                        return MA_ERROR;
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
                        else
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

                                // Remove support for HE-AAC components (SBR or PS)
                                if (pM4a->frameInfo.sbr || pM4a->frameInfo.ps)
                                {
                                        // HE-AAC detected (either SBR or PS is present), skip processing
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
                if (pM4a == NULL || pM4a->hDecoder == NULL)
                {
                        return MA_INVALID_ARGS;
                }

                // Set the current sample to the requested frame index
                pM4a->current_sample = (uint32_t)frameIndex;

                if (pM4a->isRawAAC)
                {
                        return MA_ERROR;
                }
                else
                {
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
        }

        MA_API ma_result m4a_decoder_get_data_format(
            m4a_decoder *pM4a,
            ma_format *pFormat,
            ma_uint32 *pChannels,
            ma_uint32 *pSampleRate,
            ma_channel *pChannelMap,
            size_t channelMapCap)
        {
                // Initialize output variables
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

                if (pM4a == NULL)
                {
                        return MA_INVALID_OPERATION;
                }

                if (!pM4a->isRawAAC)
                {
                        if (pM4a->track == NULL)
                        {
                                return MA_INVALID_OPERATION;
                        }
                }

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

                if (pChannels != NULL)
                {
                        *pChannels = pM4a->channels;
                }

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
