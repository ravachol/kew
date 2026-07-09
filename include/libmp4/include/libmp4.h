/**
 * Copyright (c) 2018 Parrot Drones SAS
 * Copyright (c) 2016 Aurelien Barre
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the copyright holders nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LIBMP4_H_
#define _LIBMP4_H_

#include <inttypes.h>
#include <json-c/json.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* To be used for all public API */
#ifdef MP4_API_EXPORTS
#	ifdef _WIN32
#		define MP4_API __declspec(dllexport)
#	else /* !_WIN32 */
#		define MP4_API __attribute__((visibility("default")))
#	endif /* !_WIN32 */
#else /* !MP4_API_EXPORTS */
#	define MP4_API
#endif /* !MP4_API_EXPORTS */


/**
 * MP4 Metadata keys for muxer.
 * Setting the META key also sets the associated UDTA key to the same value,
 * unless previously set.
 * Setting the UDTA key also sets the associated META key to the same value,
 * unless previously set.
 */
#define MP4_META_KEY_FRIENDLY_NAME "com.apple.quicktime.artist"
#define MP4_UDTA_KEY_FRIENDLY_NAME "\251ART"
#define MP4_META_KEY_TITLE "com.apple.quicktime.title"
#define MP4_UDTA_KEY_TITLE "\251nam"
#define MP4_META_KEY_COMMENT "com.apple.quicktime.comment"
#define MP4_UDTA_KEY_COMMENT "\251cmt"
#define MP4_META_KEY_COPYRIGHT "com.apple.quicktime.copyright"
#define MP4_UDTA_KEY_COPYRIGHT "\251cpy"
#define MP4_META_KEY_MEDIA_DATE "com.apple.quicktime.creationdate"
#define MP4_UDTA_KEY_MEDIA_DATE "\251day"
#define MP4_META_KEY_LOCATION "com.apple.quicktime.location.ISO6709"
#define MP4_UDTA_KEY_LOCATION "\251xyz"
#define MP4_META_KEY_MAKER "com.apple.quicktime.make"
#define MP4_UDTA_KEY_MAKER "\251mak"
#define MP4_META_KEY_MODEL "com.apple.quicktime.model"
#define MP4_UDTA_KEY_MODEL "\251mod"
#define MP4_META_KEY_SOFTWARE_VERSION "com.apple.quicktime.software"
#define MP4_UDTA_KEY_SOFTWARE_VERSION "\251swr"

#define MP4_MUX_DEFAULT_TABLE_SIZE_MB 2

enum mp4_track_type {
	MP4_TRACK_TYPE_UNKNOWN = 0,
	MP4_TRACK_TYPE_VIDEO,
	MP4_TRACK_TYPE_AUDIO,
	MP4_TRACK_TYPE_HINT,
	MP4_TRACK_TYPE_METADATA,
	MP4_TRACK_TYPE_TEXT,
	MP4_TRACK_TYPE_CHAPTERS,
};


enum mp4_video_codec {
	MP4_VIDEO_CODEC_UNKNOWN = 0,
	MP4_VIDEO_CODEC_AVC,
	MP4_VIDEO_CODEC_HEVC,
};


enum mp4_audio_codec {
	MP4_AUDIO_CODEC_UNKNOWN = 0,
	MP4_AUDIO_CODEC_AAC_LC,
};


enum mp4_metadata_cover_type {
	MP4_METADATA_COVER_TYPE_UNKNOWN = 0,
	MP4_METADATA_COVER_TYPE_JPEG,
	MP4_METADATA_COVER_TYPE_PNG,
	MP4_METADATA_COVER_TYPE_BMP,
};


enum mp4_seek_method {
	/* Seek to the previous (<) sample */
	MP4_SEEK_METHOD_PREVIOUS = 0,
	/* Seek to the previous (<) sync sample */
	MP4_SEEK_METHOD_PREVIOUS_SYNC,
	/* Seek to the nearest sample */
	MP4_SEEK_METHOD_NEAREST,
	/* Seek to the nearest sync sample */
	MP4_SEEK_METHOD_NEAREST_SYNC,
	/* Seek to the next (>) sample */
	MP4_SEEK_METHOD_NEXT,
	/* Seek to the next (>) sync sample */
	MP4_SEEK_METHOD_NEXT_SYNC,
};


struct mp4_media_info {
	uint64_t duration;
	uint64_t creation_time;
	uint64_t modification_time;
	uint32_t track_count;
};


struct mp4_track_info {
	uint32_t id;
	const char *name;
	int enabled;
	int in_movie;
	int in_preview;
	enum mp4_track_type type;
	uint32_t timescale;
	uint64_t duration;
	uint64_t creation_time;
	uint64_t modification_time;
	uint32_t sample_count;
	uint32_t sample_max_size;
	const uint64_t *sample_offsets;
	const uint32_t *sample_sizes;
	enum mp4_video_codec video_codec;
	uint32_t video_width;
	uint32_t video_height;
	enum mp4_audio_codec audio_codec;
	uint32_t audio_channel_count;
	uint32_t audio_sample_size;
	float audio_sample_rate;
	const char *content_encoding;
	const char *mime_format;
	int has_metadata;
	const char *metadata_content_encoding;
	const char *metadata_mime_format;
};


/* hvcC box structure */
struct mp4_hvcc_info {
	uint8_t general_profile_space;
	uint8_t general_tier_flag;
	uint8_t general_profile_idc;
	uint32_t general_profile_compatibility_flags;
	uint64_t general_constraints_indicator_flags;
	uint8_t general_level_idc;
	uint16_t min_spatial_segmentation_idc;
	uint8_t parallelism_type;
	uint8_t chroma_format;
	uint8_t bit_depth_luma;
	uint8_t bit_depth_chroma;
	uint16_t avg_framerate;
	uint8_t constant_framerate;
	uint8_t num_temporal_layers;
	uint8_t temporal_id_nested;
	uint8_t length_size;
};


struct mp4_video_decoder_config {
	enum mp4_video_codec codec;
	union {
		struct {
			union {
				uint8_t *sps;
				const uint8_t *c_sps;
			};
			size_t sps_size;
			union {
				uint8_t *pps;
				const uint8_t *c_pps;
			};
			size_t pps_size;
		} avc;
		struct {
			struct mp4_hvcc_info hvcc_info;
			union {
				uint8_t *vps;
				const uint8_t *c_vps;
			};
			size_t vps_size;
			union {
				uint8_t *sps;
				const uint8_t *c_sps;
			};
			size_t sps_size;
			union {
				uint8_t *pps;
				const uint8_t *c_pps;
			};
			size_t pps_size;
		} hevc;
	};
	uint32_t width;
	uint32_t height;
};


struct mp4_track_sample {
	uint32_t size;
	uint64_t offset;
	uint32_t metadata_size;
	int silent;
	int sync;
	uint64_t dts;
	uint64_t next_dts;
	uint64_t prev_sync_dts;
	uint64_t next_sync_dts;
};


struct mp4_mux_track_params {
	/* Track type */
	enum mp4_track_type type;
	/* Track name, if NULL, an empty string will be used */
	const char *name;
	/* Track flags (bool-like) */
	int enabled;
	int in_movie;
	int in_preview;
	/* Track timescale, mandatory */
	uint32_t timescale;
	/* Creation time */
	uint64_t creation_time;
	/* Modification time. If zero, creation time will be used */
	uint64_t modification_time;
};


struct mp4_mux_sample {
	const uint8_t *buffer;
	size_t len;
	int sync;
	int64_t dts;
};


struct mp4_mux_scattered_sample {
	const uint8_t *const *buffers;
	const size_t *len;
	int nbuffers;
	int sync;
	int64_t dts;
};


/* Demuxer API */

struct mp4_demux;


/**
 * Create an MP4 demuxer.
 * The instance handle is returned through the mp4_demux parameter.
 * When no longer needed, the instance must be freed using the
 * mp4_demux_close() function.
 * @param filename: file path to use
 * @param ret_obj: demuxer instance handle (output)
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int mp4_demux_open(const char *filename, struct mp4_demux **ret_obj);


/**
 * Free an MP4 demuxer.
 * This function frees all resources associated with a demuxer instance.
 * @param demux: demuxer instance handle
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int mp4_demux_close(struct mp4_demux *demux);


/**
 * Get the media level information.
 * @param demux: demuxer instance handle
 * @param media_info: pointer to the media_info structure (output)
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int mp4_demux_get_media_info(const struct mp4_demux *demux,
				     struct mp4_media_info *media_info);


/**
 * Get the number of tracks of an MP4 demuxer.
 * @param demux: demuxer instance handle
 * @return the track count on success, negative errno value in case of error
 */
MP4_API int mp4_demux_get_track_count(const struct mp4_demux *demux);


/**
 * Get the info of a specific track.
 * @param demux: demuxer instance handle
 * @param track_idx: track index
 * @param track_info: pointer to the track_info structure to fill (output)
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int mp4_demux_get_track_info(const struct mp4_demux *demux,
				     unsigned int track_idx,
				     struct mp4_track_info *track_info);


/**
 * Get the video decoder config of a specific track.
 * @param demux: demuxer instance handle
 * @param track_id: track ID
 * @param vdc: pointer to the video_decoder_config structure to fill
 * (output)
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int
mp4_demux_get_track_video_decoder_config(const struct mp4_demux *demux,
					 unsigned int track_id,
					 struct mp4_video_decoder_config *vdc);


/**
 * Get the audio config of a specific track.
 * @param demux: demuxer instance handle
 * @param track_id: track ID
 * @param audio_specific_config: pointer to the audio specific config buffer
 * (output)
 * @param asc_size: pointer to the size of the audio specific config (output)
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int
mp4_demux_get_track_audio_specific_config(const struct mp4_demux *demux,
					  unsigned int track_id,
					  uint8_t **audio_specific_config,
					  unsigned int *asc_size);


/**
 * Get a track sample.
 * @param demux: demuxer instance handle
 * @param track_id: track ID
 * @param advance: if true, advance to the next sample of the track
 * @param sample_buffer: sample buffer (optional, can be null)
 * @param sample_buffer_size: sample buffer size
 * @param metadata_buffer: metadata buffer (optional, can be null)
 * @param metadata_buffer_size: size of the metadata buffer
 * @param track_sample: pointer to the track_sample structure to fill (output)
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int mp4_demux_get_track_sample(const struct mp4_demux *demux,
				       unsigned int track_id,
				       int advance,
				       uint8_t *sample_buffer,
				       unsigned int sample_buffer_size,
				       uint8_t *metadata_buffer,
				       unsigned int metadata_buffer_size,
				       struct mp4_track_sample *track_sample);


/**
 * Get the previous sample time of a track.
 * @param demux: demuxer instance handle
 * @param track_id: track ID
 * @param sample_time: pointer to the sample time to fill (output)
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int mp4_demux_get_track_prev_sample_time(const struct mp4_demux *demux,
						 unsigned int track_id,
						 uint64_t *sample_time);


/**
 * Get the next sample time of a track.
 * @param demux: demuxer instance handle
 * @param track_id: track ID
 * @param sample_time: pointer to the sample time to fill (output)
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int mp4_demux_get_track_next_sample_time(const struct mp4_demux *demux,
						 unsigned int track_id,
						 uint64_t *sample_time);


/**
 * Get the previous sample time of a track before a timestamp.
 * @param demux: demuxer instance handle
 * @param track_id: track ID
 * @param time: timestamp
 * @param sync: if true, search for a sync sample
 * @param sample_time: pointer to the sample time to fill (output)
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int
mp4_demux_get_track_prev_sample_time_before(const struct mp4_demux *demux,
					    unsigned int track_id,
					    uint64_t time,
					    int sync,
					    uint64_t *sample_time);


/**
 * Get the next sample time of a track before a timestamp.
 * @param demux: demuxer instance handle
 * @param track_id: track ID
 * @param time: timestamp
 * @param sync: if true, search for a sync sample
 * @param sample_time (output): sample time
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int
mp4_demux_get_track_next_sample_time_after(const struct mp4_demux *demux,
					   unsigned int track_id,
					   uint64_t time,
					   int sync,
					   uint64_t *sample_time);


/**
 * Seek to a time offset.
 * @param demux: demuxer instance handle
 * @param time_offset: timestamp
 * @param method: seek method
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int mp4_demux_seek(const struct mp4_demux *demux,
			   uint64_t time_offset,
			   enum mp4_seek_method method);


/**
 * Seek to the previous sample of a track.
 * @param demux: demuxer instance handle
 * @param track_id: track ID
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int mp4_demux_seek_to_track_prev_sample(const struct mp4_demux *demux,
						unsigned int track_id);


/**
 * Seek to the next sample of a track.
 * @param demux: demuxer instance handle
 * @param track_id: track ID
 * @param resync: if set, seek to the previous sync point and silent frames
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int mp4_demux_seek_to_track_next_sample(const struct mp4_demux *demux,
						unsigned int track_id,
						bool resync);


/**
 * Get the chapters of an MP4 file.
 * @param demux: demuxer instance handle
 * @param chapters_count: pointer to the chapters count to fill (output)
 * @param chapters_time: pointer to chapters times array to fill (output)
 * @param chapters_name: pointer to a chapters names array to fill (output)
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int mp4_demux_get_chapters(struct mp4_demux *demux,
				   unsigned int *chapters_count,
				   uint64_t **chapters_time,
				   char ***chapters_name);


/**
 * Get the metadata strings of an MP4 file.
 * @param demux: demuxer instance handle
 * @param count: pointer to the metadata count to fill (output)
 * @param keys: pointer to the metadata keys array to fill (output)
 * @param values: pointer to the metadata values array to fill (output)
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int mp4_demux_get_metadata_strings(struct mp4_demux *demux,
					   unsigned int *count,
					   char ***keys,
					   char ***values);


/**
 * Get the metadata strings of a track.
 * @param demux: demuxer instance handle
 * @param track_id: track ID
 * @param count: pointer to the metadata count to fill (output)
 * @param keys: pointer to the metadata keys array to fill (output)
 * @param values: pointer to the metadata values array to fill (output)
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int mp4_demux_get_track_metadata_strings(const struct mp4_demux *demux,
						 unsigned int track_id,
						 unsigned int *count,
						 char ***keys,
						 char ***values);


/**
 * Get the metadata cover of an MP4 file.
 * @param demux: demuxer instance handle
 * @param cover_buffer: pointer to the cover data to fill (output)
 * @param cover_buffer_size: cover buffer size
 * @param cover_size: pointer to the cover data size to fill (output)
 * @param cover_type: pointer to the cover type to fill (output)
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int
mp4_demux_get_metadata_cover(const struct mp4_demux *demux,
			     uint8_t *cover_buffer,
			     unsigned int cover_buffer_size,
			     unsigned int *cover_size,
			     enum mp4_metadata_cover_type *cover_type);


/* Muxer API */

struct mp4_mux;

struct mp4_mux_config {
	const char *filename;
	mode_t filemode;
	uint32_t timescale;
	uint64_t creation_time;
	uint64_t modification_time;
	size_t tables_size_mbytes;
	struct {
		/* will be created by mp4_mux_open, must be deleted by caller
		 * after calling mp4_mux_close */
		const char *tables_file;
		bool check_storage_uuid;
		/* If enabled, preallocate the recovery tables file.
		 * The file referenced by mux->recovery.fd_tables will have its
		 * size extended to mux->data_offset bytes. This reserves enough
		 * space to store the recovery tables without further
		 * reallocations during muxing.
		 * Note: only the tables file is preallocated, not the main MP4
		 * file. */
		bool allocate_space_for_tables_file;
	} recovery;
};


/**
 * Create an MP4 muxer.
 * The instance handle is returned through the mp4_mux parameter.
 * When no longer needed, the instance must be freed using the
 * mp4_mux_close() function.
 * @note mp4_recovery_finalize must be called after mp4_mux_close if recovery is
 * enabled
 * @param config: mp4_mux_config to use
 * @param ret_obj: muxer instance handle (output)
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int mp4_mux_open(const struct mp4_mux_config *config,
			 struct mp4_mux **ret_obj);


/**
 * Sync an MP4 muxer.
 * If recovery is enabled, this function must be called periodically to write
 * the tables in the recovery files.
 * If write_tables parameter is set to true, the tables are written in the final
 * file (if recovery is enabled, tables will also be written in the recovery
 * files).
 * @note sync with write_tables allows the MP4 file to be read at any time but
 * the operation requires to fully write the tables; sync without write_tables
 * only writes the tables after the last call to mp4_mux_sync but requires a
 * recovery with mp4_recovery_recover_file to read the MP4 file.
 * @param mux: muxer instance handle
 * @param write_tables: if true, tables are written in the final file.
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int mp4_mux_sync(struct mp4_mux *mux, bool write_tables);


/**
 * Free an MP4 muxer.
 * This function frees all resources associated with a muxer instance.
 * @param mux: muxer instance handle
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int mp4_mux_close(struct mp4_mux *mux);


/**
 * Add a track in an MP4 muxer.
 * @param mux: muxer instance handle
 * @param params: mp4_mux_track_params of the track to add
 * @return the new track count on success, negative errno value in case of error
 */
MP4_API int mp4_mux_add_track(struct mp4_mux *mux,
			      const struct mp4_mux_track_params *params);


/**
 * Add a reference to a track.
 * @param mux: muxer instance handle
 * @param track_handle: track handle
 * @param ref_track_handle: reference track handle
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int mp4_mux_add_ref_to_track(const struct mp4_mux *mux,
				     uint32_t track_handle,
				     uint32_t ref_track_handle);


/**
 * Set the video decoder config of a track.
 * @param mux: muxer instance handle
 * @param track_handle: track handle
 * @param vdc: mp4_video_decoder_config to set
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int mp4_mux_track_set_video_decoder_config(
	const struct mp4_mux *mux,
	int track_handle,
	const struct mp4_video_decoder_config *vdc);


/**
 * Set the audio specific config of a track.
 * @param mux: muxer instance handle
 * @param track_handle: track handle
 * @param asc: buffer containing the audio specific config to set
 * @param asc_size: size of the buffer containing the audio specific config
 * @param channel_count: channel count
 * @param sample_size: sample size
 * @param sample_rate: sample rate
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int mp4_mux_track_set_audio_specific_config(const struct mp4_mux *mux,
						    int track_handle,
						    const uint8_t *asc,
						    size_t asc_size,
						    uint32_t channel_count,
						    uint32_t sample_size,
						    float sample_rate);


/**
 * Set the metadata mime type of a track.
 * @param mux: muxer instance handle
 * @param track_handle: track handle
 * @param content_encoding: content encoding
 * @param mime_type: mime type
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int mp4_mux_track_set_metadata_mime_type(const struct mp4_mux *mux,
						 int track_handle,
						 const char *content_encoding,
						 const char *mime_type);


/**
 * Add a file level metadata.
 * @param mux: muxer instance handle
 * @param key: metadata key
 * @param value: metadata value
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int mp4_mux_add_file_metadata(struct mp4_mux *mux,
				      const char *key,
				      const char *value);


/**
 * Add a track level metadata.
 * @param mux: muxer instance handle
 * @param track_handle: track handle
 * @param key: metadata key
 * @param value: metadata value
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int mp4_mux_add_track_metadata(struct mp4_mux *mux,
				       uint32_t track_handle,
				       const char *key,
				       const char *value);


/**
 * Set the cover of an MP4 file.
 * @param mux: muxer instance handle
 * @param cover_type: type of the cover
 * @param cover: cover data
 * @param cover_size: size of cover
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int mp4_mux_set_file_cover(struct mp4_mux *mux,
				   enum mp4_metadata_cover_type cover_type,
				   const uint8_t *cover,
				   size_t cover_size);


/**
 * Add a sample to a track.
 * @param mux: muxer instance handle
 * @param track_handle: track handle
 * @param sample: sample to add
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int mp4_mux_track_add_sample(const struct mp4_mux *mux,
				     int track_handle,
				     const struct mp4_mux_sample *sample);


/**
 * Add a scattered sample to a track.
 * @param mux: muxer instance handle
 * @param track_handle: track handle
 * @param sample: sample to add
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int mp4_mux_track_add_scattered_sample(
	const struct mp4_mux *mux,
	int track_handle,
	const struct mp4_mux_scattered_sample *sample);


/**
 * Print the muxer data.
 * @param mux: muxer instance handle
 * @return 0 on success, negative errno value in case of error
 */
MP4_API void mp4_mux_dump(struct mp4_mux *mux);


/* Utilities */

/**
 * Create an avc decoder config.
 * @param sps: buffer containing the sps
 * @param sps_size: size of the buffer containing the sps
 * @param pps: buffer containing the pps
 * @param pps_size: size of the buffer containing the pps
 * @param avcc: pointer to the avc decoder config buffer (output)
 * @param avcc_size: pointer to the avc decoder config buffer size (output)
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int mp4_generate_avc_decoder_config(const uint8_t *sps,
					    unsigned int sps_size,
					    const uint8_t *pps,
					    unsigned int pps_size,
					    uint8_t *avcc,
					    unsigned int *avcc_size);


/**
 * Create a chapter sample.
 * @param chapter_str: name of the chapter
 * @param buffer: pointer to the chapter sample buffer (output)
 * @param buffer_size: pointer to the chapter sample buffer size (output)
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int mp4_generate_chapter_sample(const char *chapter_str,
					uint8_t **buffer,
					unsigned int *buffer_size);


/**
 * Get a string from an enum mp4_track_type value.
 * @param type: track type value to convert
 * @return a string description of the track type
 */
MP4_API const char *mp4_track_type_str(enum mp4_track_type type);


/**
 * Get a string from an enum mp4_video_codec value.
 * @param codec: video codec value to convert
 * @return a string description of the video codec
 */
MP4_API const char *mp4_video_codec_str(enum mp4_video_codec codec);


/**
 * Get a string from an enum mp4_audio_codec value.
 * @param codec: audio codec value to convert
 * @return a string description of the audio codec
 */
MP4_API const char *mp4_audio_codec_str(enum mp4_audio_codec codec);


/**
 * Get a string from an enum mp4_metadata_cover_type value.
 * @param type: metadata cover type value to convert
 * @return a string description of the cover type
 */
MP4_API const char *
mp4_metadata_cover_type_str(enum mp4_metadata_cover_type type);


static inline uint64_t mp4_usec_to_sample_time(uint64_t time,
					       uint32_t timescale)
{
	return (time * timescale + 500000) / 1000000;
}


static inline uint64_t mp4_sample_time_to_usec(uint64_t time,
					       uint32_t timescale)
{
	if (timescale == 0)
		return 0;
	return (time * 1000000 + timescale / 2) / timescale;
}


static inline uint64_t mp4_convert_timescale(uint64_t time,
					     uint32_t src_timescale,
					     uint32_t dest_timescale)
{
	if (src_timescale == dest_timescale)
		return time;
	return (time * dest_timescale + src_timescale / 2) / src_timescale;
}


/**
 * Convert an MP4 file to a json object
 * @param filename: file path
 * @param verbose: if true, print all the MP4 boxes
 * @param json_obj: pointer to the json_object to fill (output)
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int mp4_file_to_json(const char *filename,
			     bool verbose,
			     struct json_object **json_obj);


/* Recovery API */

#define MP4_RECOVERY_TABLES_HEADER_MAGIC 0x5245434F


struct mp4_recovery_tables_header {
	/* 'RECO' (0x5245434F) */
	uint32_t magic;
	/* Recovery version */
	uint32_t version;
	/* Size of the written tables */
	uint64_t tables_size;
	/* Reserved size for tables in data file */
	uint64_t mux_tables_size;
	/* Path of the data file */
	char *data_path;
	/* Length of the path */
	uint32_t data_path_length;
	/* UUID of the data file (or empty if no check of UUID)	*/
	char *uuid;
	/* Length of the UUID */
	uint32_t uuid_length;
};


/**
 * Fills a mp4_recovery_tables_header structure from a tables_file fd.
 * @param tables_file_fd: tables_file fd
 * @param header: mp4_recovery_tables_header structure to fill
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int
mp4_recovery_tables_header_read_fd(int tables_file_fd,
				   struct mp4_recovery_tables_header *header);


/**
 * Fills a mp4_recovery_tables_header structure from a tables_file path.
 * @param tables_file: tables_file path
 * @param header: mp4_recovery_tables_header structure to fill
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int
mp4_recovery_tables_header_read_file(const char *tables_file,
				     struct mp4_recovery_tables_header *header);


/**
 * Clean up a mp4_recovery_tables_header structure.
 * @param header: the mp4_recovery_tables_header structure to clean up
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int
mp4_recovery_tables_header_clear(struct mp4_recovery_tables_header *header);


/**
 * Recovery function.
 * Must be called after mp4_recovery_recover_file or after
 * a call to mp4_mux_close.
 * Remove the tables_file
 * @param tables_file: tables file path used for recovery.
 * @param truncate_file: if true, will truncate the media file to 0
 * byte.
 * @param override_data_path: (optional, can be null) if NULL, uses the path
 * stored in the table header. Use this to specify a local path when recovering
 * files on a different system
 * @return 0 on success, negative errno value in case of error
 */
MP4_API
int mp4_recovery_finalize(const char *tables_file,
			  bool truncate_file,
			  const char *override_data_path);


/**
 * Recovery function.
 * Use the tables_file to find a data file (where mdat is written) and a
 * tables file (where moov is written) and merge the two into a valid
 * MP4 file.
 * @param tables_file: tables file path to use for recovery.
 * @param error_msg (output): required, unset on success, a string
 * description of the failure, the string should be freed after usage
 * @param recovered_file (output): path of the recovered file (optional), must
 * be released by caller after use.
 * @return 0 on success, negative errno value in case of error
 */
MP4_API
int mp4_recovery_recover_file(const char *tables_file,
			      char **error_msg,
			      char **recovered_file);


/**
 * Recovery function.
 * Rewrite the tables_file with the custom data_file (where mdat is written)
 * parameters, and merge the two into a valid MP4 file.
 * @param tables_file: tables file to use
 * before starting the recovery.
 * @param data_file: custom data path that will written in the tables file
 * before starting the recovery.
 * @param error_msg (output): required, unset on success, a string
 * description of the failure, the string should be freed after usage
 * @param recovered_file (output): path of the recovered file (optional), must
 * be released by caller after use.
 * @return 0 on success, negative errno value in case of error
 */
MP4_API
int mp4_recovery_recover_file_from_paths(const char *tables_file,
					 const char *data_file,
					 char **error_msg,
					 char **recovered_file);


/**
 * Recovery function.
 * Dump information contained in the tables file.
 * @param tables_file: tables file to use
 * before starting the recovery.
 * @return 0 on success, negative errno value in case of error
 */
MP4_API int mp4_recovery_dump_tables_file(const char *tables_file);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_LIBMP4_H_ */
